/*

Created by GEMINI CLI

*/

#include "Monitor.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

ConsoleMonitor& ConsoleMonitor::Get() {
    static ConsoleMonitor instance;
    return instance;
}

ConsoleMonitor::ConsoleMonitor() {
    _hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    _hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    
    _hBuffer[0] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    _hBuffer[1] = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(_hBuffer[0], &cursorInfo);
    SetConsoleCursorInfo(_hBuffer[1], &cursorInfo);

    _lastPpsTime = std::chrono::steady_clock::now();
}

ConsoleMonitor::~ConsoleMonitor() {
    Stop();
    CloseHandle(_hBuffer[0]);
    CloseHandle(_hBuffer[1]);
}

void ConsoleMonitor::Start() {
    if (_isRunning) return;
    _isRunning = true;
    _renderThread = std::make_unique<std::thread>(&ConsoleMonitor::RenderThread, this);
    _inputThread = std::make_unique<std::thread>(&ConsoleMonitor::InputThread, this);
}

void ConsoleMonitor::Stop() {
    _isRunning = false;
    
    // 입력 스레드 깨우기 (강제로 종료시키기 위해 엔터 입력 시늉을 낼 수도 있지만, 여기선 detach 혹은 join)
    // CancelSynchronousIo(_inputThread->native_handle()); // 위험할 수 있음
    // 단순히 join을 기다리면 입력이 올 때까지 블락될 수 있음.
    // 하지만 메인 로직에서 _isRunning을 false로 만들고 왔다면 이미 엔터를 친 상태일 것임.
    
    if (_inputThread && _inputThread->joinable()) {
        // 입력 스레드가 ReadConsoleInput에 막혀있을 수 있음.
        // 강제 종료보다는, 메인에서 엔터를 쳐서 끝내는 시나리오이므로 join 가능.
        // 만약 프로그램 종료(Ctrl+C)라면?
         _inputThread->detach(); // 안전하게 detach
    }

    if (_renderThread && _renderThread->joinable()) {
        _renderThread->join();
    }

    SetConsoleActiveScreenBuffer(_hConsoleOut);
}

void ConsoleMonitor::AddLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(_logMutex);
    _logs.push_back(msg);
    if (_logs.size() > _maxLogs) {
        _logs.pop_front();
    }
}

void ConsoleMonitor::UpdateClientCount(int count) { _clientCount = count; }
void ConsoleMonitor::UpdateLatency(int ms) 
{
    _totalLatency += ms;
    _latencyCount++;

    _avgLatency = _totalLatency / _latencyCount; // 총 Latency에 대한 평균
}

void ConsoleMonitor::IncrementTcpPacket() { _tcpPacketCounter++; }
void ConsoleMonitor::IncrementUdpPacket() { _udpPacketCounter++; }

void ConsoleMonitor::UpdateErrorRate() 
{
    // 
}

void ConsoleMonitor::UpdateStatus() {
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastPpsTime).count();
    
    if (diff >= 1000) { // 1초마다 갱신
        _tcpPps = (_tcpPacketCounter.exchange(0) * 1000 / diff);
        _udpPps = (_udpPacketCounter.exchange(0) * 1000 / diff);

        UpdateErrorRate();
        _lastPpsTime = now;
    }
}

void ConsoleMonitor::InputThread() {
    INPUT_RECORD irInBuf[128];
    DWORD cNumRead;

    while (_isRunning) {
        if (!ReadConsoleInput(_hConsoleIn, irInBuf, 128, &cNumRead)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        for (DWORD i = 0; i < cNumRead; i++) {
            if (irInBuf[i].EventType == KEY_EVENT && irInBuf[i].Event.KeyEvent.bKeyDown) {
                if (irInBuf[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
                    _isRunning = false;
                    return;
                }
            }
        }
    }
}

void ConsoleMonitor::RenderThread() {
    while (_isRunning) {
        UpdateStatus();
        Draw();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 FPS
    }
}

void ConsoleMonitor::Draw() {
    HANDLE hBackBuffer = _hBuffer[1 - _currentBufferIndex];
    
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hBackBuffer, &csbi);
    COORD bufferSize = { csbi.srWindow.Right - csbi.srWindow.Left + 1, csbi.srWindow.Bottom - csbi.srWindow.Top + 1 };
    
    if (bufferSize.X <= 0 || bufferSize.Y <= 0) return;

    std::vector<CHAR_INFO> bufferData(bufferSize.X * bufferSize.Y);
    for (auto& ci : bufferData) {
        ci.Char.UnicodeChar = L' ';
        ci.Attributes = FOREGROUND_WHITE;
    }

    // 하단 통계 영역 높이 (예: 4줄)
    int statsHeight = 5;
    int logAreaHeight = bufferSize.Y - statsHeight;
    if (logAreaHeight < 0) logAreaHeight = 0;

    // 1. 로그 그리기
    {
        std::lock_guard<std::mutex> lock(_logMutex);
        int logIdx = (int)_logs.size() - 1;
        int screenRow = logAreaHeight - 1;

        while (logIdx >= 0 && screenRow >= 0) {
            std::string text = _logs[logIdx];
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
                text.pop_back();
            }

            if (text.empty()) {
                screenRow--;
            } else {
                int len = (int)text.length();
                int width = bufferSize.X;

                for (int start = (len - 1) / width * width; start >= 0; start -= width) {
                    if (screenRow < 0) break;
                    std::string chunk = text.substr(start, width);
                    for (size_t i = 0; i < chunk.length(); ++i) {
                        int idx = screenRow * bufferSize.X + i;
                        if (idx < bufferData.size()) {
                            bufferData[idx].Char.UnicodeChar = chunk[i];
                            bufferData[idx].Attributes = FOREGROUND_WHITE;
                        }
                    }
                    screenRow--;
                }
            }
            logIdx--;
        }
    }

    // 2. 구분선
    int lineRow = logAreaHeight;
    if (lineRow < bufferSize.Y) {
        for (int x = 0; x < bufferSize.X; ++x) {
            int idx = lineRow * bufferSize.X + x;
            if (idx < bufferData.size()) {
                bufferData[idx].Char.UnicodeChar = L'=' ;
                bufferData[idx].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            }
        }
    }

    // 3. 통계 정보 그리기
    int statsStartRow = lineRow + 1;
    
    auto DrawStatLine = [&](int rowOffset, const std::wstring& label, const std::wstring& value) {
        int r = statsStartRow + rowOffset;
        if (r >= bufferSize.Y) return;
        
        std::wstring fullText = label + L": " + value;
        for (size_t i = 0; i < fullText.length(); ++i) {
            int idx = r * bufferSize.X + i + 2; // 왼쪽 여백 2
            if (idx < bufferData.size()) {
                bufferData[idx].Char.UnicodeChar = fullText[i];
                bufferData[idx].Attributes = FOREGROUND_WHITE | FOREGROUND_INTENSITY;
            }
        }
    };

    DrawStatLine(0, L"Connected Clients", std::to_wstring(_clientCount));
    
    std::wstringstream ssLatency;
    ssLatency << std::fixed << std::setprecision(2) << _avgLatency.load() << L" ms";
    DrawStatLine(1, L"Average Latency", ssLatency.str());

    std::wstringstream ssError;
    ssError << std::fixed << std::setprecision(2) << _errorRate.load() << L" %";
    DrawStatLine(2, L"Error Rate", ssError.str());

    std::wstringstream ssPps;
    ssPps << L"TCP: " << _tcpPps << L" / UDP: " << _udpPps;
    DrawStatLine(3, L"Packet/Sec", ssPps.str());

    // Help Text
    int helpRow = statsStartRow + 4;
    if (helpRow < bufferSize.Y) {
        std::wstring helpText = L" [SYSTEM] Monitor Active. Press ENTER to exit.";
        for (size_t i = 0; i < helpText.length(); ++i) {
            int idx = helpRow * bufferSize.X + i;
            if (idx < bufferData.size()) {
                bufferData[idx].Char.UnicodeChar = helpText[i];
                bufferData[idx].Attributes = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            }
        }
    }

    // 4. 출력 및 버퍼 교체
    COORD bufferSizeCoord = { (SHORT)bufferSize.X, (SHORT)bufferSize.Y };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, (SHORT)(bufferSize.X - 1), (SHORT)(bufferSize.Y - 1) };

    WriteConsoleOutput(hBackBuffer, bufferData.data(), bufferSizeCoord, bufferCoord, &writeRegion);
    SetConsoleActiveScreenBuffer(hBackBuffer);
    _currentBufferIndex = 1 - _currentBufferIndex;
}