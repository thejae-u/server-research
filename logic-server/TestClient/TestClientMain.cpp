/*

Made with GEMINI CLI

*/

#define WIN32_LEAN_AND_MEAN
#include "VirtualClient.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "implot.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <numeric>
#include <map>
#include <random>
#include <iostream>
#include <cstdio>
#include <psapi.h>
#include <algorithm>
#include <format>

using ll = long long;

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Network Global
boost::asio::io_context io_context;
std::vector<std::shared_ptr<VirtualClient>> clients;
std::thread networkThread;
boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard = boost::asio::make_work_guard(io_context);

// groups caching
std::mutex groupsMapMutex;
std::unordered_map<std::string, std::vector<std::string>> groupsMap;

std::mutex packetHistoryMutex;
std::list<SHistory> packetHistory;
void EnqueueHistory(SHistory history);

double GetMemoryUsage();

ll gTotalRtt = 0;
ll gRttCount = 0;
ll gMaxRtt = 0;
ll gMinRtt = INT_MAX;
double gAvgRtt = 0;

// Logic
static void SpawnClients(int count, int groupMaxCount)
{
    if (count <= 0) return;

    boost::uuids::random_generator uuid_generator;
    std::string currentGroupId = boost::uuids::to_string(uuid_generator()); // Start with a new group ID
    
    int groupIndex = 1;
    int clientIndexInGroup = 0;

    for (int i = 0; i < count; ++i)
    {
        // create new group id per groupMaxCount
        if (i > 0 && i % groupMaxCount == 0)
        {
            currentGroupId = boost::uuids::to_string(uuid_generator()); 
            groupIndex++;
            clientIndexInGroup = 0;
        }
        clientIndexInGroup++;

        auto client = std::make_shared<VirtualClient>(io_context, i, "127.0.0.1", 53200, currentGroupId, groupIndex, clientIndexInGroup);
        client->Start(EnqueueHistory);

        std::lock_guard<std::mutex> groupLock(groupsMapMutex);
        groupsMap[currentGroupId].push_back(client->GetUuid());

        clients.push_back(client);
    }
}

static void StopAllClients() 
{
    for (auto& client : clients) 
    {
        client->Stop();
    }

    clients.clear();
    groupsMap.clear();
    packetHistory.clear();

    gTotalRtt = 0;
    gMinRtt = INT_MAX;
    gAvgRtt = 0.0;
    gMaxRtt = 0;
}

// Main code
int main(int, char**)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hWnd = ::CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hWnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Start Network Thread
    networkThread = std::thread([]()
    {
        io_context.run();
    });

    // State
    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    
    int targetClientCount = 10;
    int groupMaxCount = 5;

    // Graph Data
    #define HISTORY_SIZE 300
    float rttHistory[HISTORY_SIZE] = { 0 };
    int historyIdx = 0;

    static std::map<std::string, bool> g_GroupVisibility;

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Collect active groups sorted by index
        std::map<int, std::string> activeGroups;
        for (const auto& client : clients)
        {
            activeGroups[client->GetGroupIndex()] = client->GetGroupId();
        }

        // 1. Sidebar (Control Center) - Left Resizable
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        
        // Constrain height to full screen, width between 200 and 80% of screen
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(200.0f, io.DisplaySize.y), 
            ImVec2(io.DisplaySize.x * 0.8f, io.DisplaySize.y)
        );
        
        // Initial size only
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.33f, io.DisplaySize.y), ImGuiCond_FirstUseEver);

        // Allow Resize, but NoMove/NoCollapse
        ImGui::Begin("Control Center", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        // Capture current width for the next window
        float currentSidebarWidth = ImGui::GetWindowWidth();

        // Group Filter
        if (ImGui::CollapsingHeader("Group Filter", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Show All")) { 
                for (const auto& [idx, gid] : activeGroups) g_GroupVisibility[gid] = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Hide All")) { 
                for (const auto& [idx, gid] : activeGroups) g_GroupVisibility[gid] = false;
            }

            ImGui::Separator();

            if (ImGui::BeginChild("GroupList", ImVec2(0, 150), true))
            {
                for (const auto& [idx, gid] : activeGroups)
                {
                    if (g_GroupVisibility.find(gid) == g_GroupVisibility.end()) { 
                        g_GroupVisibility[gid] = true;
                    }

                    bool visible = g_GroupVisibility[gid];
                    std::string label = std::format("Group {} ({})", idx, gid.substr(0, 8));
                    if (ImGui::Checkbox(label.c_str(), &visible)) {
                        g_GroupVisibility[gid] = visible;
                    }
                }
                ImGui::EndChild();
            }
        }

        // Control Panel
        if (ImGui::CollapsingHeader("Control Panel", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Server: 127.0.0.1:53200");
            static bool isConsoleVisible = false;
            static bool isConsoleAllocated = false;

            if (ImGui::Button(isConsoleVisible ? "Hide Debug Console" : "Show Debug Console"))
            {
                if (isConsoleVisible)
                {
                    // Hide Console Window
                    HWND hConsole = ::GetConsoleWindow();
                    if (hConsole) ::ShowWindow(hConsole, SW_HIDE);
                    isConsoleVisible = false;
                }
                else
                {
                    // Allocate Console if not exists
                    if (!isConsoleAllocated)
                    {
                        if (::AllocConsole())
                        {
                            FILE* fp;
                            freopen_s(&fp, "CONOUT$", "w", stdout);
                            freopen_s(&fp, "CONOUT$", "w", stderr);
                            freopen_s(&fp, "CONIN$", "r", stdin);

                            std::cout.clear();
                            std::cerr.clear();
                            std::cin.clear();

                            // Disable 'X' button on Console Window to prevent accidental exit
                            HWND hConsoleWnd = ::GetConsoleWindow();
                            if (hConsoleWnd)
                            {
                                HMENU hMenu = ::GetSystemMenu(hConsoleWnd, FALSE);
                                if (hMenu) ::DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
                            }

                            std::cout << "Debug Console\n";
                            isConsoleAllocated = true;
                        }
                    }

                    // Show Console Window
                    HWND hConsole = ::GetConsoleWindow();
                    if (hConsole)
                    {
                        ::ShowWindow(hConsole, SW_SHOW);
                        ::SetForegroundWindow(hConsole);
                    }
                    isConsoleVisible = true;
                }
            }

            ImGui::InputInt("Client Count", &targetClientCount, 2, 500);
            ImGui::InputInt("Client per Group", &groupMaxCount, 5, 500);

            if (ImGui::Button("Spawn Clients"))
            {
                SpawnClients(targetClientCount, groupMaxCount);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop All"))
            {
                StopAllClients();
            }

            ImGui::Separator();
            ImGui::Text("Active Clients: %d", clients.size());
            ImGui::Text("Active Groups: %d", groupsMap.size());
            ImGui::Text("Memory: %.2f MB", GetMemoryUsage());

            // Calculate Stats
            int connectedCount = 0;
            int handshakeCount = 0;

            for (auto& c : clients)
            {
                auto s = c->GetState();
                if (s == ClientState::Connected) connectedCount++;
                else if (s != ClientState::Disconnected && s != ClientState::Error) handshakeCount++;

                auto stats = c->GetStats();
                if (stats.rttMs > 0)
                {
                    gTotalRtt += stats.rttMs;
                    gRttCount++;
                    gMaxRtt = std::max(gMaxRtt, (ll)stats.rttMs);
                    gMinRtt = std::min(gMinRtt, (ll)stats.rttMs);
                }
            }

            gAvgRtt = gRttCount > 0 ? (float)gTotalRtt / gRttCount : 0.0f;

            // Update History
            rttHistory[historyIdx] = gAvgRtt;
            historyIdx = (historyIdx + 1) % HISTORY_SIZE;

            ImGui::Text("Connected: %d", connectedCount);
            ImGui::Text("Handshaking: %d", handshakeCount);

            // Rtt History
            if (ImGui::BeginTable("Rtt history", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Min Rtt");
                ImGui::TableSetupColumn("Avg Rtt");
                ImGui::TableSetupColumn("Max Rtt");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", gMinRtt);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2lf", gAvgRtt);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", gMaxRtt);
                ImGui::EndTable();
            }

            // Throughput Table
            double totalTxBps = 0.0;
            double totalRxBps = 0.0;
            for(const auto& c : clients)
            {
                auto s = c->GetStats();
                totalTxBps += s.txBps;
                totalRxBps += s.rxBps;
            }

            if (ImGui::BeginTable("Throughput", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Upload");
                ImGui::TableSetupColumn("Download");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.2f MB/s", totalTxBps / (1024.0 * 1024.0));

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f MB/s", totalRxBps / (1024.0 * 1024.0));
                
                ImGui::EndTable();
            }
        }

        // Traffic Control
        if (ImGui::CollapsingHeader("Traffic Control", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SeparatorText("Move Simulation");
            static int moveIntervalMs = 16;
            ImGui::InputInt("Move Interval (ms)", &moveIntervalMs, 10, 5000);

            if (ImGui::Button("Start Move"))
            {
                for (auto& c : clients)
                {
                    c->StartRandomUdpTraffic(moveIntervalMs);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Move"))
            {
                for (auto& c : clients)
                {
                    c->StopRandomUdpTraffic();
                }
            }

            ImGui::SeparatorText("Attack Simulation");
            static int atkIntervalMs = 33;
            ImGui::InputInt("Atk Interval (ms)", &atkIntervalMs, 10, 5000);

            if (ImGui::Button("Start Attack"))
            {
                int clientSize = (int)clients.size();
                if (clientSize > 1)
                {
                    for (int i = 0; i < clientSize; ++i)
                    {
                        const auto& groupId = clients[i]->GetGroupId();

                        std::lock_guard<std::mutex> groupsLock(groupsMapMutex);
                        const auto& clientsInGroupIt = groupsMap.find(groupId);

                        if (clientsInGroupIt == groupsMap.end())
                            continue;

                        clients[i]->StartRandomAtkTraffic(atkIntervalMs, clientsInGroupIt->second);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Attack"))
            {
                for (auto& c : clients)
                {
                    c->StopRandomAtkTraffic();
                }
            }
        }

       // Packet History
        if (ImGui::CollapsingHeader("Packet History"))
        {
            if (ImGui::BeginChild("PacketLog", ImVec2(0, 200), true))
            {
                if (ImGui::BeginTable("histories", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Group");
                    ImGui::TableSetupColumn("User");
                    ImGui::TableSetupColumn("Method");
                    ImGui::TableSetupColumn("Data");
                    ImGui::TableSetupColumn("Time");
                    ImGui::TableHeadersRow();

                    std::lock_guard<std::mutex> historyLock(packetHistoryMutex);
                    for (const auto& curHistory : packetHistory)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", curHistory.groupId.substr(0, 8).c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", curHistory.userId.substr(0, 8).c_str());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", curHistory.method.c_str());

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", curHistory.data.c_str());

                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%lld", curHistory.time.time_since_epoch().count() % 100000);
                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        }

        // Client List
        if (ImGui::CollapsingHeader("Client List"))
        {
            if (ImGui::BeginChild("ClientList", ImVec2(0, 200), true))
            {
                if (ImGui::BeginTable("Clients", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("RTT");
                    ImGui::TableSetupColumn("UserId");
                    ImGui::TableSetupColumn("GroupId");
                    ImGui::TableHeadersRow();

                    int clientCounts = clients.size();
                    for (int i = 0; i < clientCounts; i++)
                    {
                        auto& client = clients[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", client->GetId());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", client->GetStateString());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d ms", client->GetStats().rttMs);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", client->GetDisplayUserId().c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%s", client->GetDisplayGroupId().c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }
        }
        ImGui::End(); // End Sidebar

        // 2. World Map (Background) - Right Remainder
        ImGui::SetNextWindowPos(ImVec2(currentSidebarWidth, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - currentSidebarWidth, io.DisplaySize.y));

        ImGui::Begin("World Map", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            ImGui::Text("World Positions (All Client)");
            ImGui::EndMenuBar();
        }

        if (ImPlot::BeginPlot("Positions", ImVec2(-1, -1), ImPlotFlags_Equal))
        {
            ImPlot::SetupAxes("X", "Z");
            ImPlot::SetupAxisLimits(ImAxis_X1, -50, 50, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -50, 50, ImGuiCond_Once);

            // Map: GroupIndex -> Pair(X list, Z list)
            std::map<int, std::pair<std::vector<float>, std::vector<float>>> simGroupPoints;
            std::map<int, std::pair<std::vector<float>, std::vector<float>>> serverGroupPoints;

            struct PointLabel { double x, z; std::string text; };
            std::vector<PointLabel> labelsToDraw;

            for (const auto& client : clients)
            {
                if (client->GetState() != ClientState::Connected) continue;

                std::string gid = client->GetGroupId();
                int gIdx = client->GetGroupIndex();

                if (g_GroupVisibility.find(gid) != g_GroupVisibility.end() && !g_GroupVisibility[gid]) {
                    continue;
                }

                auto [srvX, serverZ] = client->GetServerPosition();
                auto& srvPoints = serverGroupPoints[gIdx];
                srvPoints.first.push_back(srvX);
                srvPoints.second.push_back(serverZ);

                auto [simX, simZ] = client->GetSimPosition();
                auto& simPoints = simGroupPoints[gIdx];
                simPoints.first.push_back(simX);
                simPoints.second.push_back(simZ);

                labelsToDraw.push_back({ (double)simX, (double)simZ, client->GetDisplayUserId() });
            }

            for (const auto& [gIdx, points] : serverGroupPoints)
            {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross, 6, ImVec4(1, 1, 1, 0.3f)); // Faint White Cross
                std::string label = std::format("group_{}_server", gIdx);
                ImPlot::PlotScatter(label.c_str(), points.first.data(), points.second.data(), (int)points.first.size());
            }

            for (const auto& [gIdx, points] : simGroupPoints)
            {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4);
                std::string label = std::format("group_{}_client", gIdx);
                ImPlot::PlotScatter(label.c_str(), points.first.data(), points.second.data(), (int)points.first.size());
            }

            for (const auto& l : labelsToDraw)
            {
                ImPlot::PlotText(l.text.c_str(), l.x, l.z, ImVec2(0, -10)); // Offset slightly above
            }

            ImPlot::EndPlot();
        }
        ImGui::End(); // End Background

        // Update Window Title with FPS
        {
            static double lastTime = 0.0;
            double currentTime = ImGui::GetTime();
            if (currentTime - lastTime > 0.5) // Update every 500ms
            {
                std::wstring title = std::format(L"Logic Server Tester with Gemini CLI - {:.1f} FPS", io.Framerate);
                ::SetWindowTextW(hWnd, title.c_str());
                lastTime = currentTime;
            }
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    io_context.stop();
    if (networkThread.joinable()) networkThread.join();
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hWnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer != 0)
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            if (g_pd3dDevice != nullptr)
            {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void EnqueueHistory(SHistory history)
{
    std::lock_guard<std::mutex> lock(packetHistoryMutex);
    if (packetHistory.size() == 100)
    {
        packetHistory.pop_back();
    }

    packetHistory.push_front(history);
}

double GetMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }

    return 0.0;
}