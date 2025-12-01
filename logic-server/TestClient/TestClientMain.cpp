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
#include <atomic>
#include <unordered_map>
#include <numeric>
#include <map>
#include <random>

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

std::unordered_map<std::string, std::vector<std::string>> groupsMap;

// Logic
void SpawnClients(int count, int groupMaxCount)
{
    if (count <= 0) return;

    boost::uuids::random_generator uuid_generator;
    std::string currentGroupId = boost::uuids::to_string(uuid_generator()); // Start with a new group ID

    for (int i = 0; i < count; ++i)
    {
            // Assign a new group ID for every 4 clients
        if (i > 0 && i % groupMaxCount == 0)
        {
            currentGroupId = boost::uuids::to_string(uuid_generator()); 
        }

        auto client = std::make_shared<VirtualClient>(io_context, i, "127.0.0.1", 53200, currentGroupId);
        client->Start();
        groupsMap[currentGroupId].push_back(client->GetUuid());

        clients.push_back(client);
    }
}

void StopAllClients() 
{
    for (auto& client : clients) 
    {
        client->Stop();
    }

    clients.clear();
    groupsMap.clear();
}

// Main code
int main(int, char**)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hWnd = ::CreateWindowW(wc.lpszClassName, L"Gemini Logic Server Tester", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

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
    int groupMaxCount = 4;
    
    // Graph Data
    #define HISTORY_SIZE 300
    float rttHistory[HISTORY_SIZE] = {0};
    int historyIdx = 0;

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

        // 1. World Map (Background)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        // Use NoBringToFrontOnFocus so clicking the map doesn't obscure floating windows
        ImGui::Begin("World Map", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
             ImGui::Text("Gemini Logic Server Tester");
             ImGui::EndMenuBar();
        }

        if (ImPlot::BeginPlot("Positions", ImVec2(-1, -1), ImPlotFlags_Equal))
        {
            ImPlot::SetupAxes("X", "Z");
            ImPlot::SetupAxisLimits(ImAxis_X1, -50, 50, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -50, 50, ImGuiCond_Once);

            // Collect data by group
            // Map: GroupId -> Pair(X list, Z list)
            std::map<std::string, std::pair<std::vector<float>, std::vector<float>>> simGroupPoints;
            std::map<std::string, std::pair<std::vector<float>, std::vector<float>>> serverGroupPoints;
            
            for (const auto& client : clients)
            {
                if (client->GetState() != ClientState::Connected) continue;
                
                // Server Position
                auto [srvX, serverZ] = client->GetServerPosition();
                auto& srvPoints = serverGroupPoints[client->GetGroupId()];
                srvPoints.first.push_back(srvX);
                srvPoints.second.push_back(serverZ);

                // Sim Position
                auto [simX, simZ] = client->GetSimPosition();
                auto& simPoints = simGroupPoints[client->GetGroupId()];
                simPoints.first.push_back(simX);
                simPoints.second.push_back(simZ);
            }

            // Plot Server Points (Faint)
            for (const auto& [groupId, points] : serverGroupPoints)
            {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross, 6, ImVec4(1, 1, 1, 0.3f)); // Faint White Cross
                std::string label = groupId + "_Server";
                ImPlot::PlotScatter(label.c_str(), points.first.data(), points.second.data(), (int)points.first.size());
            }

            // Plot Sim Points (Vivid)
            for (const auto& [groupId, points] : simGroupPoints)
            {
                // Use default colored circle
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4); 
                ImPlot::PlotScatter(groupId.c_str(), points.first.data(), points.second.data(), (int)points.first.size());
            }

            ImPlot::EndPlot();
        }
        ImGui::End(); // End Background

        // 2. Control Panel
        if (ImGui::Begin("Control Panel"))
        {
            ImGui::Text("Server: 127.0.0.1:53200");

            ImGui::InputInt("Client Count", &targetClientCount, 2, 500);
            ImGui::InputInt("Group Max Count", &groupMaxCount, 4, 500);

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

            // Calculate Stats
            int connectedCount = 0;
            int handshakeCount = 0;
            long long totalRtt = 0;
            int maxRtt = 0;
            int minRtt = INT_MAX;
            int rttCount = 0;

            for (auto& c : clients)
            {
                auto s = c->GetState();
                if (s == ClientState::Connected) connectedCount++;
                else if (s != ClientState::Disconnected && s != ClientState::Error) handshakeCount++;

                auto stats = c->GetStats();
                if (stats.rttMs > 0)
                {
                    totalRtt += stats.rttMs;
                    rttCount++;
                    maxRtt = std::max(maxRtt, stats.rttMs);
                    minRtt = std::min(minRtt, stats.rttMs);
                }
            }

            float avgRtt = rttCount > 0 ? (float)totalRtt / rttCount : 0.0f;

            // Update History
            rttHistory[historyIdx] = avgRtt;
            historyIdx = (historyIdx + 1) % HISTORY_SIZE;

            ImGui::Text("Connected: %d", connectedCount);
            ImGui::Text("Handshaking: %d", handshakeCount);
            //ImGui::Text("Avg RTT: %.2f ms", avgRtt);
            if (ImGui::BeginTable("Rtt history", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Min Rtt");
                ImGui::TableSetupColumn("Avg Rtt");
                ImGui::TableSetupColumn("Max Rtt");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", minRtt);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", avgRtt);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", maxRtt);
            }

            ImGui::EndTable();
        }
        ImGui::End();
        
        // 3. Dashboard
        if (ImGui::Begin("Dashboard"))
        {
            if (ImPlot::BeginPlot("Average RTT (ms)"))
            {
                ImPlot::SetupAxes("Time", "RTT");
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 50, ImGuiCond_Once);
                
                // Flatten history for simple plotting (not efficient but simple)
                float plotData[HISTORY_SIZE];
                for(int i=0; i<HISTORY_SIZE; ++i)
                {
                    plotData[i] = rttHistory[(historyIdx + i) % HISTORY_SIZE];
                }
                
                ImPlot::PlotLine("RTT", plotData, HISTORY_SIZE);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();
        
        // 4. Client List
        if (ImGui::Begin("Client List"))
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
                    ImGui::Text("%s", client->GetUuid().c_str());
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%s", client->GetGroupId().c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        // 5. UDP Traffic Control
        if (ImGui::Begin("UDP Traffic Control"))
        {
            ImGui::SeparatorText("Move Simulation");
            static int moveIntervalMs = 100;
            ImGui::InputInt("Move Interval (ms)", &moveIntervalMs, 10, 1000);

            if (ImGui::Button("Start Move"))
            {
                for(auto& c : clients)
                {
                    c->StartRandomUdpTraffic(moveIntervalMs);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Move"))
            {
                for(auto& c : clients)
                {
                    c->StopRandomUdpTraffic();
                }
            }

            ImGui::SeparatorText("Attack Simulation");
            static int atkIntervalMs = 1000;
            ImGui::InputInt("Atk Interval (ms)", &atkIntervalMs, 100, 5000);

            if (ImGui::Button("Start Attack"))
            {
                std::random_device rd;
                std::mt19937 gen(rd());
                
                int clientSize = (int)clients.size();
                if (clientSize > 1)
                {
                    for(int i=0; i<clientSize; ++i)
                    {
                        auto groupId = clients[i]->GetGroupId();
                        auto clientsInGroup = groupsMap[groupId];

                        std::uniform_int_distribution<> dis(0, clientsInGroup.size() - 1);
                        auto targetId = clientsInGroup.at(dis(gen));

                        clients[i]->StartRandomAtkTraffic(atkIntervalMs, targetId);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Attack"))
            {
                for(auto& c : clients)
                {
                    c->StopRandomAtkTraffic();
                }
            }
        }
        ImGui::End();

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
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
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