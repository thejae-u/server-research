/*

Created by GEMINI CLI

*/

#pragma once
#include <windows.h>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include "spdlog/sinks/base_sink.h"

#define FOREGROUND_WHITE FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN

// 콘솔 모니터링 클래스 (싱글톤)
class ConsoleMonitor {
public:
    static ConsoleMonitor& Get();

    void Start();
    void Stop();
    bool IsRunning() const { return _isRunning; }

    // 로그 추가 (Sink에서 호출)
    void AddLog(const std::string& msg);

    // 통계 업데이트 메서드들
    void UpdateClientCount(int count);
    void UpdateLatency(int ms);
    void UpdateErrorRate();
    void IncrementTcpPacket();
    void IncrementUdpPacket();

private:
    ConsoleMonitor();
    ~ConsoleMonitor();

    void RenderThread();
    void InputThread();
    void Draw();
    void UpdateStatus(); // 초당 패킷 수 계산, Error Rate 계산

    // Win32 Console Handles
    HANDLE _hConsoleOut;
    HANDLE _hConsoleIn;
    HANDLE _hBuffer[2];
    int _currentBufferIndex = 0;
    
    // Threading
    std::atomic<bool> _isRunning = false;
    std::unique_ptr<std::thread> _renderThread;
    std::unique_ptr<std::thread> _inputThread;

    // Data
    std::deque<std::string> _logs;
    std::mutex _logMutex;
    const size_t _maxLogs = 50;

    // Stats (Atomic for thread safety)
    std::atomic<int> _clientCount = 0;
    std::atomic<int> _latencyCount = 0;
    std::atomic<int> _totalLatency = 0;
    std::atomic<double> _avgLatency = 0.0;
    std::atomic<double> _errorRate = 0.0; // 전송과 수신에 대해 count 계산 필요

    // PPS Calculation
    std::atomic<long long> _tcpPacketCounter = 0;
    std::atomic<long long> _udpPacketCounter = 0;
    std::atomic<int> _tcpPps = 0;
    std::atomic<int> _udpPps = 0;
    std::chrono::steady_clock::time_point _lastPpsTime;
};

// spdlog 커스텀 Sink (색상 없이 텍스트만 전달)
template<typename Mutex>
class MonitorSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        ConsoleMonitor::Get().AddLog(fmt::to_string(formatted));
    }

    void flush_() override {}
};

using MonitorSink_mt = MonitorSink<std::mutex>;
