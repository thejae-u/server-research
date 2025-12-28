/*

Made with GEMINI CLI

*/

#include "VirtualClient.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>
#include <iostream>
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
#include <cstdio>
#include <algorithm>
#include <format>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <climits> 

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

using ll = long long;

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

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

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
    if (clients.empty()) return;

    // --- Statistics Gathering & CSV Export Start ---

    // 1. Prepare Directory
    std::filesystem::path dataDir = "data";
    if (!std::filesystem::exists(dataDir))
    {
        std::filesystem::create_directory(dataDir);
    }

    // 2. Prepare Filename with Timestamp
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    std::tm bt{};
#ifdef _WIN32
    localtime_s(&bt, &in_time_t); 
#else
    localtime_r(&in_time_t, &bt);
#endif
    ss << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S");
    std::string filename = std::format("data/stats_{}.csv", ss.str());

    // 3. Calculate Stats In-Memory
    struct GroupStats {
        ll minRtt = INT_MAX;
        ll maxRtt = 0;
        ll totalRtt = 0;
        ll count = 0;
        ll txPackets = 0;
        ll rxPackets = 0;
        int clientCount = 0;
    };
    std::map<std::string, GroupStats> groupStatsMap;

    int highestClientMaxRtt = 0;
    double highestClientAvgRtt = 0.0;

    for (auto& client : clients)
    {
        auto stats = client->GetStats();

        // Update overall client max stats
        if (stats.maxRtt > highestClientMaxRtt) highestClientMaxRtt = stats.maxRtt;
        double avg = stats.GetAvgRtt();
        if (avg > highestClientAvgRtt) highestClientAvgRtt = avg;

        // Aggregating for Group Stats
        auto& gStats = groupStatsMap[client->GetGroupId()];
        gStats.clientCount++;

        // Count packets regardless of RTT status
        gStats.txPackets += stats.txPackets;
        gStats.rxPackets += stats.rxPackets;

        if (stats.rttCount > 0) // Only consider valid RTTs for latency stats
        {
            if (stats.minRtt < gStats.minRtt) gStats.minRtt = stats.minRtt;
            if (stats.maxRtt > gStats.maxRtt) gStats.maxRtt = stats.maxRtt;
            gStats.totalRtt += stats.totalRtt;
            gStats.count += stats.rttCount;
        }
    }

    double highestGroupAvgRtt = 0.0;
    double totalClientsInGroups = 0;
    for (const auto& [groupId, gStats] : groupStatsMap)
    {
        double avg = (gStats.count > 0) ? (double)gStats.totalRtt / gStats.count : 0.0;
        if (avg > highestGroupAvgRtt) highestGroupAvgRtt = avg;
        totalClientsInGroups += gStats.clientCount;
    }
    double avgClientsPerGroup = groupStatsMap.empty() ? 0.0 : totalClientsInGroups / groupStatsMap.size();

    // 4. Write to CSV
    std::ofstream csvFile(filename);
    if (csvFile.is_open())
    {
        // --- Overall Summary ---
        csvFile << "--- Overall Summary ---\n";
        csvFile << "Total Client Count," << clients.size() << "\n";
        csvFile << "Total Group Count," << groupStatsMap.size() << "\n";
        csvFile << "Clients per Group," << avgClientsPerGroup << "\n";
        csvFile << "Highest Client Max RTT," << highestClientMaxRtt << "\n";
        csvFile << "Highest Client Avg RTT," << highestClientAvgRtt << "\n";
        csvFile << "Highest Group Avg RTT," << highestGroupAvgRtt << "\n";
        csvFile << "\n";

        // --- Client Stats ---
        csvFile << "--- Client Stats ---\n";
        csvFile << "ClientID,GroupID,MinRTT,AvgRTT,MaxRTT,TxPackets,RxPackets\n";

        for (auto& client : clients)
        {
            auto stats = client->GetStats();
            int validMinRtt = (stats.minRtt == INT_MAX) ? 0 : stats.minRtt;

            csvFile << client->GetId() << "," << client->GetGroupId() << ","
                << validMinRtt << "," << stats.GetAvgRtt() << "," << stats.maxRtt << ","
                << stats.txPackets << "," << stats.rxPackets << "\n";
        }
        csvFile << "\n";

        // --- Group Stats ---
        csvFile << "--- Group Stats ---\n";
        csvFile << "GroupID,ClientCount,MinRTT,AvgRTT,MaxRTT,TotalTxPackets,TotalRxPackets\n";

        for (const auto& [groupId, gStats] : groupStatsMap)
        {
            double avg = (gStats.count > 0) ? (double)gStats.totalRtt / gStats.count : 0.0;
            ll minR = (gStats.minRtt == INT_MAX) ? 0 : gStats.minRtt;

            csvFile << groupId << ","
                << gStats.clientCount << ","
                << minR << ","
                << avg << ","
                << gStats.maxRtt << ","
                << gStats.txPackets << ","
                << gStats.rxPackets << "\n";
        }

        csvFile.close();
        std::cout << "Statistics saved to " << filename << std::endl;
    }
    else
    {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
    }
    // --- Statistics Gathering End ---

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
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decisions on OpenGL version
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "ImGui Example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
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
#ifdef _WIN32
            // Windows-specific Console Handling
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
#endif

            ImGui::InputInt("Client Count", &targetClientCount, 2, 500);
            ImGui::InputInt("Client per Group", &groupMaxCount, 5, 500);

            if (ImGui::Button("Spawn Clients"))
            {
                targetClientCount = std::min(targetClientCount, 500);
                groupMaxCount = std::min(groupMaxCount, 500);
                SpawnClients(targetClientCount, groupMaxCount);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop All"))
            {
                StopAllClients();
            }

            ImGui::Separator();
            ImGui::Text("Active Clients: %llu", clients.size());
            ImGui::Text("Active Groups: %llu", groupsMap.size());
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
                ImGui::Text("%lld", gMinRtt);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2lf", gAvgRtt);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%lld", gMaxRtt);
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
                if (ImGui::BeginTable("Clients", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("UserId");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("Avg RTT");
                    ImGui::TableSetupColumn("Send/Receive Udp Packets");
                    ImGui::TableHeadersRow();

                    int clientCounts = clients.size();
                    for (int i = 0; i < clientCounts; i++)
                    {
                        auto& client = clients[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", client->GetDisplayUserId().c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", client->GetStateString());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%.1f ms", client->GetStats().GetAvgRtt());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("S : %lld / R: %lld", client->GetStats().txPackets, client->GetStats().rxPackets);
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

                if (g_GroupVisibility.find(gid) != g_GroupVisibility.end() && !g_GroupVisibility[gid])
                    continue;

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
            double currentTime = glfwGetTime();
            if (currentTime - lastTime > 0.5) // Update every 500ms
            {
                std::string title = std::format("Logic Server Tester with Gemini CLI (OpenGL) - {:.1f} FPS", io.Framerate);
                glfwSetWindowTitle(window, title.c_str());
                lastTime = currentTime;
            }
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    io_context.stop();
    if (networkThread.joinable()) networkThread.join();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
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
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    // TODO: Linux implementation
    return 0.0;
}
