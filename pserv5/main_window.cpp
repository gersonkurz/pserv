#include "precomp.h"
#include "main_window.h"
#include "Resource.h"
#include "Config/settings.h"
#include "utils/win32_error.h"
#include "windows_api/service_manager.h"
#include "models/service_info.h"
#include "core/async_operation.h"
#include "controllers/services_data_controller.h"
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include <thread>
#include <chrono>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace pserv {

MainWindow::MainWindow() {
    m_pServicesController = new ServicesDataController();
}

MainWindow::~MainWindow() {
    if (m_pAsyncOp) {
        m_pAsyncOp->RequestCancel();
        m_pAsyncOp->Wait();
        delete m_pAsyncOp;
    }
    delete m_pServicesController;
    ClearServices();
    CleanupImGui();
    CleanupDirectX();
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

void MainWindow::ClearServices() {
    for (auto* service : m_services) {
        delete service;
    }
    m_services.clear();
}

bool MainWindow::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Register window class
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PSERV5));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PSERV5);
    wcex.lpszClassName = L"pserv5WindowClass";
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    if (!RegisterClassExW(&wcex)) {
        spdlog::error("Failed to register window class");
        return false;
    }

    // Load window settings
    auto& windowSettings = config::theSettings.window;
    int width = windowSettings.width.get();
    int height = windowSettings.height.get();
    int posX = windowSettings.positionX.get();
    int posY = windowSettings.positionY.get();
    bool maximized = windowSettings.maximized.get();

    // Create window
    m_hWnd = CreateWindowW(
        L"pserv5WindowClass",
        L"pserv5",
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        width, height,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (!m_hWnd) {
        spdlog::error("Failed to create window");
        return false;
    }

    // Store 'this' pointer for WndProc access
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Initialize DirectX 11
    if (!InitializeDirectX()) {
        spdlog::error("Failed to initialize DirectX 11");
        return false;
    }

    // Initialize ImGui
    if (!InitializeImGui()) {
        spdlog::error("Failed to initialize ImGui");
        return false;
    }

    // Load active tab from settings
    auto& appSettings = config::theSettings.application;
    m_activeTab = appSettings.activeView.get();
    spdlog::info("Loaded active tab: {}", m_activeTab);

    // Initial refresh of services
    try {
        m_pServicesController->Refresh();
        spdlog::info("Initial services refresh completed");
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh services on startup: {}", e.what());
    }

    spdlog::info("Main window initialized successfully");
    return true;
}

void MainWindow::Show(int nCmdShow) {
    // Override nCmdShow if we have a saved maximized state
    auto& windowSettings = config::theSettings.window;
    if (windowSettings.maximized.get()) {
        nCmdShow = SW_SHOWMAXIMIZED;
    }
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
}

int MainWindow::MessageLoop() {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Render frame when idle
            Render();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Forward messages to ImGui first
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    // Retrieve MainWindow instance pointer
    MainWindow* pWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message) {
    case WM_SIZE:
        if (pWindow && pWindow->m_pDevice && wParam != SIZE_MINIMIZED) {
            pWindow->CleanupRenderTarget();
            pWindow->m_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            pWindow->CreateRenderTarget();
        }
        return 0;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_ASYNC_OPERATION_COMPLETE:
        if (pWindow) {
            pWindow->m_bShowProgressDialog = false;
            if (pWindow->m_pAsyncOp) {
                auto status = pWindow->m_pAsyncOp->GetStatus();
                if (status == AsyncStatus::Completed) {
                    spdlog::info("Async operation completed successfully");
                } else if (status == AsyncStatus::Cancelled) {
                    spdlog::info("Async operation was cancelled");
                } else if (status == AsyncStatus::Failed) {
                    spdlog::error("Async operation failed: {}", pWindow->m_pAsyncOp->GetErrorMessage());
                }
            }
        }
        return 0;

    case WM_DESTROY:
        if (pWindow) {
            pWindow->SaveWindowState();
            pWindow->SaveServicesTableState(true); // Force save on window close
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

bool MainWindow::InitializeDirectX() {
    // Create swap chain description
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pDevice,
        &featureLevel,
        &m_pDeviceContext
    );

    if (FAILED(hr)) {
        spdlog::error("D3D11CreateDeviceAndSwapChain failed: {:#x}", static_cast<unsigned int>(hr));
        return false;
    }

    CreateRenderTarget();
    spdlog::info("DirectX 11 initialized successfully");
    return true;
}

void MainWindow::CleanupDirectX() {
    CleanupRenderTarget();
    m_pSwapChain.Reset();
    m_pDeviceContext.Reset();
    m_pDevice.Reset();
}

void MainWindow::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRenderTargetView);
    }
}

void MainWindow::CleanupRenderTarget() {
    m_pRenderTargetView.Reset();
}

bool MainWindow::InitializeImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Load custom font - try Segoe UI first (Windows standard), fall back to Arial
    wchar_t windowsDir[MAX_PATH];
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) == 0) {
        spdlog::warn("GetWindowsDirectory failed, using default font");
        io.Fonts->AddFontDefault();
    } else {
        std::wstring fontsDir = std::wstring(windowsDir) + L"\\Fonts\\";
        const wchar_t* fontFiles[] = {
            L"segoeui.ttf",  // Segoe UI (modern, clean)
            L"arial.ttf"     // Arial (fallback)
        };

        bool fontLoaded = false;
        for (const wchar_t* fontFile : fontFiles) {
            std::wstring fontPath = fontsDir + fontFile;
            // Convert to narrow string for ImGui
            int size = WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string narrowPath(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, &narrowPath[0], size, nullptr, nullptr);
            narrowPath.resize(size - 1); // Remove null terminator

            if (io.Fonts->AddFontFromFileTTF(narrowPath.c_str(), 16.0f)) {
                spdlog::info("Loaded font: {}", narrowPath);
                fontLoaded = true;
                break;
            }
        }

        if (!fontLoaded) {
            spdlog::warn("Could not load custom font, using default");
            io.Fonts->AddFontDefault();
        }
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_pDevice.Get(), m_pDeviceContext.Get());

    spdlog::info("ImGui initialized successfully");
    return true;
}

void MainWindow::CleanupImGui() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

void MainWindow::RenderProgressDialog() {
    if (!m_bShowProgressDialog || !m_pAsyncOp) {
        return;
    }

    ImGui::OpenPopup("Operation in Progress");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Operation in Progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        float progress = m_pAsyncOp->GetProgress();
        std::string message = m_pAsyncOp->GetProgressMessage();

        ImGui::Text("%s", message.c_str());
        ImGui::ProgressBar(progress, ImVec2(400, 0));
        ImGui::Text("%.1f%%", progress * 100.0f);

        ImGui::Separator();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pAsyncOp->RequestCancel();
            spdlog::info("User requested cancellation");
        }

        ImGui::EndPopup();
    }
}

void MainWindow::Render() {
    if (!m_pDeviceContext || !m_pRenderTargetView) {
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create fullscreen window for tab bar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);

    // Create tab bar with placeholder tabs
    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
        const char* tabs[] = {"Services", "Devices", "Processes", "Windows", "Modules", "Uninstaller"};
        static bool firstFrame = true;

        for (const char* tab : tabs) {
            // Set the initially active tab on first frame
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (firstFrame && m_activeTab == tab) {
                flags |= ImGuiTabItemFlags_SetSelected;
            }

            if (ImGui::BeginTabItem(tab, nullptr, flags)) {
                // Check if active tab changed
                if (m_activeTab != tab) {
                    m_activeTab = tab;
                    auto& appSettings = config::theSettings.application;
                    appSettings.activeView.set(m_activeTab);
                    if (m_pConfigBackend) {
                        appSettings.save(*m_pConfigBackend);
                        spdlog::debug("Active tab changed to: {}", m_activeTab);
                    }
                }

                // Services view
                if (std::string(tab) == "Services") {
                    ImGui::Separator();
                    if (ImGui::Button("Refresh Services")) {
                        try {
                            m_pServicesController->Refresh();
                        } catch (const std::exception& e) {
                            spdlog::error("Failed to refresh services: {}", e.what());
                        }
                    }

                    const auto& services = m_pServicesController->GetServices();
                    ImGui::SameLine();
                    ImGui::Text("Services: %zu", services.size());
                    ImGui::Separator();

                    // Display services in a table
                    const auto& columns = m_pServicesController->GetColumns();
                    ImGuiTableFlags flags = ImGuiTableFlags_Sortable |
                                           ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_Borders |
                                           ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_Reorderable |
                                           ImGuiTableFlags_Hideable |
                                           ImGuiTableFlags_ScrollX |
                                           ImGuiTableFlags_ScrollY |
                                           ImGuiTableFlags_SizingFixedFit;

                    if (ImGui::BeginTable("ServicesTable", static_cast<int>(columns.size()), flags)) {
                        // Cache the table pointer for saving state later
                        m_pServicesTable = ImGui::GetCurrentTable();

                        // Load saved column widths from config
                        static std::vector<float> columnWidths;
                        static bool widthsLoaded = false;
                        if (!widthsLoaded) {
                            std::string widthsStr = config::theSettings.servicesTable.columnWidths.get();
                            spdlog::debug("Loading column widths from config: {}", widthsStr);
                            std::istringstream iss(widthsStr);
                            std::string token;
                            while (std::getline(iss, token, ',')) {
                                columnWidths.push_back(std::stof(token));
                            }
                            widthsLoaded = true;
                            spdlog::debug("Loaded {} column widths", columnWidths.size());
                        }

                        // Ensure we have the correct number of widths
                        while (columnWidths.size() < columns.size()) {
                            columnWidths.push_back(100.0f);
                        }

                        // Setup columns with loaded widths - must be in storage order (0,1,2,3,4)
                        ImGui::TableSetupColumn(columns[0].DisplayName.c_str(), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, columnWidths[0], 0);
                        ImGui::TableSetupColumn(columns[1].DisplayName.c_str(), ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, columnWidths[1], 1);
                        ImGui::TableSetupColumn(columns[2].DisplayName.c_str(), ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, columnWidths[2], 2);
                        ImGui::TableSetupColumn(columns[3].DisplayName.c_str(), ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, columnWidths[3], 3);
                        ImGui::TableSetupColumn(columns[4].DisplayName.c_str(), ImGuiTableColumnFlags_None | ImGuiTableColumnFlags_WidthFixed, columnWidths[4], 4);

                        // Apply saved column order AFTER TableSetupColumn but BEFORE TableHeadersRow
                        static bool orderApplied = false;
                        if (!orderApplied) {
                            ImGuiTable* table = ImGui::GetCurrentTable();
                            std::string orderStr = config::theSettings.servicesTable.columnOrder.get();
                            spdlog::debug("Restoring column order: {}", orderStr);

                            std::istringstream iss(orderStr);
                            std::string token;
                            std::vector<int> displayOrder;
                            while (std::getline(iss, token, ',')) {
                                displayOrder.push_back(std::stoi(token));
                            }

                            // Apply DisplayOrder to columns and rebuild DisplayOrderToIndex
                            if (displayOrder.size() == columns.size()) {
                                spdlog::debug("Before restoration:");
                                for (int i = 0; i < table->ColumnsCount; ++i) {
                                    spdlog::debug("  Column[{}]: DisplayOrder={}, IndexWithinEnabledSet={}",
                                        i, table->Columns[i].DisplayOrder, table->Columns[i].IndexWithinEnabledSet);
                                }

                                for (size_t i = 0; i < displayOrder.size(); ++i) {
                                    if (i < table->ColumnsCount) {
                                        table->Columns[i].DisplayOrder = static_cast<ImGuiTableColumnIdx>(displayOrder[i]);
                                    }
                                }

                                // Rebuild DisplayOrderToIndex mapping
                                for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
                                    table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = static_cast<ImGuiTableColumnIdx>(column_n);
                                }

                                spdlog::debug("After restoration:");
                                for (int i = 0; i < table->ColumnsCount; ++i) {
                                    spdlog::debug("  Column[{}]: DisplayOrder={}, IndexWithinEnabledSet={}",
                                        i, table->Columns[i].DisplayOrder, table->Columns[i].IndexWithinEnabledSet);
                                }
                            }

                            orderApplied = true;
                        }

                        ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row
                        ImGui::TableHeadersRow();

                        // Check for sorting
                        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
                            if (sortSpecs->SpecsDirty) {
                                // Sort is requested
                                if (sortSpecs->SpecsCount > 0) {
                                    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                                    int columnIndex = spec.ColumnIndex;
                                    bool ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);

                                    m_pServicesController->Sort(columnIndex, ascending);
                                }
                                sortSpecs->SpecsDirty = false;
                            }
                        }

                        // Display rows
                        for (const auto* service : services) {
                            ImGui::TableNextRow();
                            for (size_t i = 0; i < columns.size(); ++i) {
                                ImGui::TableSetColumnIndex(static_cast<int>(i));
                                std::string value = service->GetProperty(static_cast<int>(i));
                                ImGui::TextUnformatted(value.c_str());
                            }
                        }

                        ImGui::EndTable();

                        // Save table state periodically (throttled inside the method)
                        SaveServicesTableState();
                    }
                }
                // Other views
                else {
                    ImGui::Text("This is the %s view", tab);
                    ImGui::Text("Content will be implemented in future milestones");
                }

                ImGui::EndTabItem();
            }
        }

        firstFrame = false;
        ImGui::EndTabBar();
    }

    ImGui::End();

    // Render progress dialog if active
    RenderProgressDialog();

    // Rendering
    ImGui::Render();

    // Clear to cornflower blue (classic DirectX default)
    const float clearColor[4] = { 0.392f, 0.584f, 0.929f, 1.0f };
    m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clearColor);

    // Render ImGui
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    m_pSwapChain->Present(1, 0); // VSync enabled
}

void MainWindow::SaveWindowState() {
    if (!m_hWnd || !m_pConfigBackend) {
        spdlog::warn("Cannot save window state: hWnd={}, pConfigBackend={}",
            (void*)m_hWnd, (void*)m_pConfigBackend);
        return;
    }

    auto& windowSettings = config::theSettings.window;

    // Check if maximized
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(m_hWnd, &wp)) {
        LogWin32Error("GetWindowPlacement");
        return;
    }

    windowSettings.maximized.set(wp.showCmd == SW_SHOWMAXIMIZED);

    // Get normal (non-maximized) position
    RECT& rc = wp.rcNormalPosition;
    windowSettings.positionX.set(rc.left);
    windowSettings.positionY.set(rc.top);
    windowSettings.width.set(rc.right - rc.left);
    windowSettings.height.set(rc.bottom - rc.top);

    // Save to config backend
    windowSettings.save(*m_pConfigBackend);

    spdlog::info("Window state saved: {}x{} at ({}, {}), maximized={}",
        windowSettings.width.get(), windowSettings.height.get(),
        windowSettings.positionX.get(), windowSettings.positionY.get(),
        windowSettings.maximized.get());
}

void MainWindow::SaveServicesTableState(bool force) {
    spdlog::debug("SaveServicesTableState called: force={}, pConfigBackend={}, pServicesTable={}",
        force, (void*)m_pConfigBackend, (void*)m_pServicesTable);

    if (!m_pConfigBackend) {
        spdlog::warn("Cannot save services table state: pConfigBackend is null");
        return;
    }

    if (!m_pServicesTable) {
        spdlog::debug("ServicesTable pointer is null, skipping save");
        return;
    }

    // Throttle saves - only save once per second (unless forced)
    static auto lastSaveTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSaveTime);
    if (!force && elapsed.count() < 1000) {
        spdlog::trace("Skipping save: throttled ({}ms elapsed)", elapsed.count());
        return; // Skip save if less than 1 second has passed
    }
    lastSaveTime = now;

    spdlog::debug("Proceeding with save");

    auto& tableSettings = config::theSettings.servicesTable;

    // Save column widths
    std::ostringstream widthsStream;
    for (int i = 0; i < m_pServicesTable->ColumnsCount; ++i) {
        if (i > 0) widthsStream << ",";
        widthsStream << m_pServicesTable->Columns[i].WidthGiven;
    }
    spdlog::debug("Column widths: {}", widthsStream.str());
    tableSettings.columnWidths.set(widthsStream.str());

    // Save column display order
    std::ostringstream orderStream;
    for (int i = 0; i < m_pServicesTable->ColumnsCount; ++i) {
        if (i > 0) orderStream << ",";
        orderStream << static_cast<int>(m_pServicesTable->Columns[i].DisplayOrder);
    }
    spdlog::debug("Column order: {}", orderStream.str());
    tableSettings.columnOrder.set(orderStream.str());

    // Save to config backend
    tableSettings.save(*m_pConfigBackend);

    spdlog::info("Services table state saved: widths={}, order={}",
        widthsStream.str(), orderStream.str());
}

} // namespace pserv
