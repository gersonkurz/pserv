#include "precomp.h"
#include "main_window.h"
#include "Resource.h"
#include "Config/settings.h"
#include "utils/win32_error.h"
#include "utils/string_utils.h"
#include "windows_api/service_manager.h"
#include "models/service_info.h"
#include "core/async_operation.h"
#include "controllers/services_data_controller.h"
#include "dialogs/service_properties_dialog.h"
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include <shellapi.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace pserv {

MainWindow::MainWindow() {
    m_pServicesController = new ServicesDataController();
    m_pPropertiesDialog = new ServicePropertiesDialog();
}

MainWindow::~MainWindow() {
    if (m_pAsyncOp) {
        m_pAsyncOp->RequestCancel();
        m_pAsyncOp->Wait();
        delete m_pAsyncOp;
    }
    delete m_pServicesController;
    delete m_pPropertiesDialog;
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
    wcex.lpszMenuName = nullptr;  // No Win32 menu - using ImGui menu instead
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

    // Create window with modern borderless style
    // WS_POPUP = no title bar, WS_THICKFRAME = resizable borders
    m_hWnd = CreateWindowW(
        L"pserv5WindowClass",
        L"pserv5",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
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
                    // Refresh services to show updated state
                    if (pWindow->m_pServicesController) {
                        pWindow->m_pServicesController->Refresh();
                    }
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

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends first
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_pDevice.Get(), m_pDeviceContext.Get());

    // Load font with size from settings (scaled by 100) - after backends are initialized
    int32_t fontSizeScaled = config::theSettings.application.fontSizeScaled.get();
    float fontSize = fontSizeScaled / 100.0f;
    RebuildFontAtlas(fontSize);

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

void MainWindow::RebuildFontAtlas(float fontSize) {
    ImGuiIO& io = ImGui::GetIO();

    // Clear existing fonts
    io.Fonts->Clear();

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

            if (io.Fonts->AddFontFromFileTTF(narrowPath.c_str(), fontSize)) {
                spdlog::info("Loaded font: {} at size {}", narrowPath, fontSize);
                fontLoaded = true;
                break;
            }
        }

        if (!fontLoaded) {
            spdlog::warn("Could not load custom font, using default");
            io.Fonts->AddFontDefault();
        }
    }

    // Rebuild font texture
    io.Fonts->Build();

    // Set the new font as default
    if (io.Fonts->Fonts.Size > 0) {
        io.FontDefault = io.Fonts->Fonts[0];

        // CRITICAL: Update ImGui's FontSizeBase so it renders at the new size
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontSizeBase = fontSize;
    } else {
        spdlog::error("No fonts available after rebuild!");
    }

    // Notify ImGui backends to update their font texture
    if (m_pDevice && m_pDeviceContext) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
    } else {
        spdlog::error("Device or context is null, cannot update font texture!");
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

    // Apply pending font size change BEFORE starting any ImGui frame
    if (m_pendingFontSize > 0.0f) {
        RebuildFontAtlas(m_pendingFontSize);
        m_pendingFontSize = 0.0f;  // Reset pending flag
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

    // Handle Ctrl+Mousewheel for font size changes
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.MouseWheel != 0.0f) {
        auto& appSettings = config::theSettings.application;
        int32_t currentSizeScaled = appSettings.fontSizeScaled.get();
        float currentSize = currentSizeScaled / 100.0f;
        float newSize = currentSize + io.MouseWheel * 1.0f;  // Change by 1 pixel per wheel notch

        // Clamp font size between 8 and 32
        newSize = std::max(8.0f, std::min(32.0f, newSize));

        if (newSize != currentSize) {
            int32_t newSizeScaled = static_cast<int32_t>(newSize * 100.0f);
            appSettings.fontSizeScaled.set(newSizeScaled);
            if (m_pConfigBackend) {
                appSettings.save(*m_pConfigBackend);
            }
            // Defer font rebuild until after frame is complete
            m_pendingFontSize = newSize;
        }
    }

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

                    // Filter input box
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(300.0f);
                    ImGui::InputTextWithHint("##filter", "Filter services...", m_filterText, IM_ARRAYSIZE(m_filterText));

                    // Filter services based on search text
                    const auto& allServices = m_pServicesController->GetServices();
                    std::vector<const ServiceInfo*> filteredServices;
                    std::string filterLower;

                    if (m_filterText[0] != '\0') {
                        // Convert filter text to lowercase for case-insensitive search
                        filterLower = m_filterText;
                        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        for (const auto* service : allServices) {
                            // Search in display name and service name
                            std::string displayName = service->GetDisplayName();
                            std::string serviceName = service->GetName();
                            std::transform(displayName.begin(), displayName.end(), displayName.begin(),
                                [](unsigned char c) { return std::tolower(c); });
                            std::transform(serviceName.begin(), serviceName.end(), serviceName.begin(),
                                [](unsigned char c) { return std::tolower(c); });

                            if (displayName.find(filterLower) != std::string::npos ||
                                serviceName.find(filterLower) != std::string::npos) {
                                filteredServices.push_back(service);
                            }
                        }
                    } else {
                        // No filter - show all services
                        filteredServices.assign(allServices.begin(), allServices.end());
                    }

                    ImGui::SameLine();
                    ImGui::Text("Services: %zu / %zu", filteredServices.size(), allServices.size());
                    ImGui::Separator();

                    // Reserve space for status bar at the bottom (one line of text + padding)
                    float statusBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
                    float tableHeight = ImGui::GetContentRegionAvail().y - statusBarHeight;

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

                    ImVec2 tableSize(0.0f, tableHeight);
                    if (ImGui::BeginTable("ServicesTable", static_cast<int>(columns.size()), flags, tableSize)) {
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

                        // Setup columns with loaded widths dynamically
                        for (size_t i = 0; i < columns.size(); ++i) {
                            ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_WidthFixed;

                            // First column gets DefaultSort flag
                            if (i == 0) {
                                columnFlags |= ImGuiTableColumnFlags_DefaultSort;
                            }

                            ImGui::TableSetupColumn(
                                columns[i].DisplayName.c_str(),
                                columnFlags,
                                columnWidths[i],
                                static_cast<ImGuiID>(i)
                            );
                        }

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
                                        table->Columns[static_cast<int>(i)].DisplayOrder = static_cast<ImGuiTableColumnIdx>(displayOrder[i]);
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
                        for (const auto* service : filteredServices) {
                            ImGui::TableNextRow();
                            ImGui::PushID(service);

                            // Get visual state from controller
                            VisualState visualState = m_pServicesController->GetVisualState(service);

                            // Apply text color based on visual state
                            if (visualState == VisualState::Disabled) {
                                // Gray out disabled items
                                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                            } else if (visualState == VisualState::Highlighted) {
                                // Highlight special items (blue-ish, theme-aware)
                                ImVec4 highlightColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                                highlightColor.x *= 0.6f; // Reduce red
                                highlightColor.y *= 0.8f; // Reduce green
                                highlightColor.z = 1.0f;  // Full blue
                                ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
                            }

                            // Display columns with context menu on right-click
                            for (size_t i = 0; i < columns.size(); ++i) {
                                ImGui::TableSetColumnIndex(static_cast<int>(i));
                                std::string value = service->GetProperty(static_cast<int>(i));

                                // First column: use selectable to make row clickable
                                if (i == 0) {
                                    // Check if this service is selected
                                    bool isSelected = std::find(m_selectedServices.begin(), m_selectedServices.end(), service) != m_selectedServices.end();

                                    if (ImGui::Selectable(value.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                                        // Handle selection logic
                                        ImGuiIO& io = ImGui::GetIO();

                                        if (io.KeyCtrl) {
                                            // Ctrl+Click: toggle selection
                                            if (isSelected) {
                                                m_selectedServices.erase(std::remove(m_selectedServices.begin(), m_selectedServices.end(), service), m_selectedServices.end());
                                            } else {
                                                m_selectedServices.push_back(service);
                                            }
                                            m_lastClickedService = service;
                                        } else if (io.KeyShift && m_lastClickedService != nullptr) {
                                            // Shift+Click: range selection
                                            // Find the range between last clicked and current
                                            auto lastIt = std::find(filteredServices.begin(), filteredServices.end(), m_lastClickedService);
                                            auto currentIt = std::find(filteredServices.begin(), filteredServices.end(), service);

                                            if (lastIt != filteredServices.end() && currentIt != filteredServices.end()) {
                                                // Clear selection first
                                                m_selectedServices.clear();

                                                // Select range
                                                auto start = (lastIt < currentIt) ? lastIt : currentIt;
                                                auto end = (lastIt < currentIt) ? currentIt : lastIt;

                                                for (auto it = start; it <= end; ++it) {
                                                    m_selectedServices.push_back(*it);
                                                }
                                            }
                                        } else {
                                            // Normal click: clear selection and select only this one
                                            m_selectedServices.clear();
                                            m_selectedServices.push_back(service);
                                            m_lastClickedService = service;
                                        }
                                    }

                                    // Context menu for selected services
                                    if (ImGui::BeginPopupContextItem()) {
                                        // If right-clicked on non-selected item, select only that one
                                        if (!isSelected) {
                                            m_selectedServices.clear();
                                            m_selectedServices.push_back(service);
                                        }

                                        // Get actions that are common to all selected services
                                        auto actions = m_pServicesController->GetAvailableActions(service);
                                        for (size_t i = 0; i < actions.size(); ++i) {
                                            const auto& action = actions[i];

                                            // Handle separator
                                            if (action == ServiceAction::Separator) {
                                                ImGui::Separator();
                                                continue;
                                            }

                                            std::string actionName = ServicesDataController::GetActionName(action);

                                            // Show count if multiple services selected (except for copy and file system actions)
                                            std::string menuLabel = actionName;
                                            if (m_selectedServices.size() > 1 &&
                                                action != ServiceAction::CopyName &&
                                                action != ServiceAction::CopyDisplayName &&
                                                action != ServiceAction::CopyBinaryPath &&
                                                action != ServiceAction::OpenInRegistryEditor &&
                                                action != ServiceAction::OpenInExplorer &&
                                                action != ServiceAction::OpenTerminalHere) {
                                                menuLabel += std::format(" ({} services)", m_selectedServices.size());
                                            }

                                            if (ImGui::MenuItem(menuLabel.c_str())) {
                                                // Handle action
                                                switch (action) {
                                                case ServiceAction::CopyName:
                                                    // Copy names of all selected services
                                                    {
                                                        std::string result;
                                                        for (const auto* svc : m_selectedServices) {
                                                            if (!result.empty()) result += "\n";
                                                            result += svc->GetName();
                                                        }
                                                        ImGui::SetClipboardText(result.c_str());
                                                        spdlog::debug("Copied {} service name(s) to clipboard", m_selectedServices.size());
                                                    }
                                                    break;

                                                case ServiceAction::CopyDisplayName:
                                                    // Copy display names of all selected services
                                                    {
                                                        std::string result;
                                                        for (const auto* svc : m_selectedServices) {
                                                            if (!result.empty()) result += "\n";
                                                            result += svc->GetDisplayName();
                                                        }
                                                        ImGui::SetClipboardText(result.c_str());
                                                        spdlog::debug("Copied {} service display name(s) to clipboard", m_selectedServices.size());
                                                    }
                                                    break;

                                                case ServiceAction::CopyBinaryPath:
                                                    // Copy binary paths of all selected services
                                                    {
                                                        std::string result;
                                                        for (const auto* svc : m_selectedServices) {
                                                            if (!result.empty()) result += "\n";
                                                            result += svc->GetBinaryPathName();
                                                        }
                                                        ImGui::SetClipboardText(result.c_str());
                                                        spdlog::debug("Copied {} service binary path(s) to clipboard", m_selectedServices.size());
                                                    }
                                                    break;

                                                case ServiceAction::Start:
                                                    // Start service(s) asynchronously
                                                    {
                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Start {} service(s)", serviceNames.size());

                                                        // Clean up previous async operation if any
                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        // Create new async operation
                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        // Start the service(s) in a worker thread
                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                                                    float progressRange = 1.0f / static_cast<float>(total);

                                                                    op->ReportProgress(baseProgress, std::format("Starting service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                    // Start service with progress callback
                                                                    ServiceManager::StartServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
                                                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                                                    });
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Started {} service(s) successfully", total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to start service: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::Stop:
                                                    // Stop service(s) asynchronously
                                                    {
                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Stop {} service(s)", serviceNames.size());

                                                        // Clean up previous async operation if any
                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        // Create new async operation
                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        // Stop the service(s) in a worker thread
                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                                                    float progressRange = 1.0f / static_cast<float>(total);

                                                                    op->ReportProgress(baseProgress, std::format("Stopping service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                    // Stop service with progress callback
                                                                    ServiceManager::StopServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
                                                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                                                    });
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Stopped {} service(s) successfully", total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to stop service: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::Pause:
                                                    // Pause service(s) asynchronously
                                                    {
                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Pause {} service(s)", serviceNames.size());

                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                                                    float progressRange = 1.0f / static_cast<float>(total);

                                                                    op->ReportProgress(baseProgress, std::format("Pausing service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                    ServiceManager::PauseServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
                                                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                                                    });
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Paused {} service(s) successfully", total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to pause service: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::Resume:
                                                    // Resume service(s) asynchronously
                                                    {
                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Resume {} service(s)", serviceNames.size());

                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                                                    float progressRange = 1.0f / static_cast<float>(total);

                                                                    op->ReportProgress(baseProgress, std::format("Resuming service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                    ServiceManager::ResumeServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
                                                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                                                    });
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Resumed {} service(s) successfully", total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to resume service: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::Restart:
                                                    // Restart service(s) asynchronously
                                                    {
                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Restart {} service(s)", serviceNames.size());

                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                                                    float progressRange = 1.0f / static_cast<float>(total);

                                                                    op->ReportProgress(baseProgress, std::format("Restarting service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                    ServiceManager::RestartServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
                                                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                                                    });
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Restarted {} service(s) successfully", total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to restart service: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::SetStartupAutomatic:
                                                case ServiceAction::SetStartupManual:
                                                case ServiceAction::SetStartupDisabled:
                                                    // Change startup type for selected service(s)
                                                    {
                                                        // Determine the target startup type
                                                        DWORD startType;
                                                        std::string startTypeName;
                                                        if (action == ServiceAction::SetStartupAutomatic) {
                                                            startType = SERVICE_AUTO_START;
                                                            startTypeName = "Automatic";
                                                        } else if (action == ServiceAction::SetStartupManual) {
                                                            startType = SERVICE_DEMAND_START;
                                                            startTypeName = "Manual";
                                                        } else {
                                                            startType = SERVICE_DISABLED;
                                                            startTypeName = "Disabled";
                                                        }

                                                        // Copy selected services list for async operation
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        spdlog::info("Starting async operation: Set startup type to {} for {} service(s)",
                                                            startTypeName, serviceNames.size());

                                                        if (m_pAsyncOp) {
                                                            m_pAsyncOp->Wait();
                                                            delete m_pAsyncOp;
                                                        }

                                                        m_pAsyncOp = new AsyncOperation();
                                                        m_bShowProgressDialog = true;

                                                        m_pAsyncOp->Start(m_hWnd, [serviceNames, startType, startTypeName](AsyncOperation* op) -> bool {
                                                            try {
                                                                size_t total = serviceNames.size();
                                                                for (size_t i = 0; i < total; ++i) {
                                                                    const std::string& serviceName = serviceNames[i];
                                                                    float progress = static_cast<float>(i) / static_cast<float>(total);

                                                                    op->ReportProgress(progress, std::format("Setting startup type for '{}'... ({}/{})",
                                                                        serviceName, i + 1, total));

                                                                    ServiceManager::ChangeServiceStartType(serviceName, startType);
                                                                }

                                                                op->ReportProgress(1.0f, std::format("Set startup type to {} for {} service(s)",
                                                                    startTypeName, total));
                                                                return true;
                                                            } catch (const std::exception& e) {
                                                                spdlog::error("Failed to change startup type: {}", e.what());
                                                                return false;
                                                            }
                                                        });
                                                    }
                                                    break;

                                                case ServiceAction::OpenInRegistryEditor:
                                                    // Open registry editor and navigate to service key
                                                    {
                                                        std::string serviceName = m_selectedServices[0]->GetName();
                                                        // Set last key in registry so regedit opens to this location
                                                        std::string regPath = std::format("SYSTEM\\CurrentControlSet\\Services\\{}", serviceName);
                                                        HKEY hKey;
                                                        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
                                                            0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                                                            std::wstring fullPath = utils::Utf8ToWide(std::format("HKEY_LOCAL_MACHINE\\{}", regPath));
                                                            RegSetValueExW(hKey, L"LastKey", 0, REG_SZ, (const BYTE*)fullPath.c_str(), (DWORD)(fullPath.length() + 1) * sizeof(wchar_t));
                                                            RegCloseKey(hKey);
                                                        }
                                                        spdlog::info("Opening registry editor for: {}", serviceName);
                                                        ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOW);
                                                    }
                                                    break;

                                                case ServiceAction::OpenInExplorer:
                                                    // Open explorer for first selected service's install location
                                                    {
                                                        std::string installLocation = m_selectedServices[0]->GetInstallLocation();
                                                        if (!installLocation.empty()) {
                                                            spdlog::info("Opening explorer: {}", installLocation);
                                                            std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
                                                            ShellExecuteW(NULL, L"open", wInstallLocation.c_str(), NULL, NULL, SW_SHOW);
                                                        } else {
                                                            spdlog::warn("No install location available for this service");
                                                        }
                                                    }
                                                    break;

                                                case ServiceAction::OpenTerminalHere:
                                                    // Open terminal in first selected service's install location
                                                    {
                                                        std::string installLocation = m_selectedServices[0]->GetInstallLocation();
                                                        if (!installLocation.empty()) {
                                                            spdlog::info("Opening terminal in: {}", installLocation);
                                                            std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
                                                            ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, wInstallLocation.c_str(), SW_SHOW);
                                                        } else {
                                                            spdlog::warn("No install location available for this service");
                                                        }
                                                    }
                                                    break;

                                                case ServiceAction::UninstallService:
                                                    // Delete selected service(s) with confirmation
                                                    {
                                                        // Copy selected services list
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        // Show confirmation dialog
                                                        std::string confirmMsg;
                                                        if (serviceNames.size() == 1) {
                                                            confirmMsg = std::format("Are you sure you want to delete the service '{}'?\n\nThis will remove the service from the system.", serviceNames[0]);
                                                        } else {
                                                            confirmMsg = std::format("Are you sure you want to delete {} services?\n\nThis will remove all selected services from the system.", serviceNames.size());
                                                        }

                                                        int result = MessageBoxA(m_hWnd, confirmMsg.c_str(), "Confirm Service Deletion", MB_YESNO | MB_ICONWARNING);
                                                        if (result == IDYES) {
                                                            spdlog::info("Starting async operation: Delete {} service(s)", serviceNames.size());

                                                            if (m_pAsyncOp) {
                                                                m_pAsyncOp->Wait();
                                                                delete m_pAsyncOp;
                                                            }

                                                            m_pAsyncOp = new AsyncOperation();
                                                            m_bShowProgressDialog = true;

                                                            m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                                try {
                                                                    size_t total = serviceNames.size();
                                                                    for (size_t i = 0; i < total; ++i) {
                                                                        const std::string& serviceName = serviceNames[i];
                                                                        float progress = static_cast<float>(i) / static_cast<float>(total);

                                                                        op->ReportProgress(progress, std::format("Deleting service '{}'... ({}/{})", serviceName, i + 1, total));

                                                                        ServiceManager::DeleteService(serviceName);
                                                                    }

                                                                    op->ReportProgress(1.0f, std::format("Deleted {} service(s) successfully", total));
                                                                    return true;
                                                                } catch (const std::exception& e) {
                                                                    spdlog::error("Failed to delete service: {}", e.what());
                                                                    return false;
                                                                }
                                                            });
                                                        }
                                                    }
                                                    break;

                                                case ServiceAction::DeleteRegistryKey:
                                                    // Delete registry key for selected service(s) with confirmation
                                                    {
                                                        // Copy selected services list
                                                        std::vector<std::string> serviceNames;
                                                        for (const auto* svc : m_selectedServices) {
                                                            serviceNames.push_back(svc->GetName());
                                                        }

                                                        // Show confirmation dialog
                                                        std::string confirmMsg;
                                                        if (serviceNames.size() == 1) {
                                                            confirmMsg = std::format("Are you sure you want to delete the registry key for service '{}'?\n\nThis will remove: HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\{}\n\nThis is typically used to clean up orphaned service registry entries.", serviceNames[0], serviceNames[0]);
                                                        } else {
                                                            confirmMsg = std::format("Are you sure you want to delete the registry keys for {} services?\n\nThis will remove the registry entries under HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\", serviceNames.size());
                                                        }

                                                        int result = MessageBoxA(m_hWnd, confirmMsg.c_str(), "Confirm Registry Key Deletion", MB_YESNO | MB_ICONWARNING);
                                                        if (result == IDYES) {
                                                            spdlog::info("Starting async operation: Delete registry keys for {} service(s)", serviceNames.size());

                                                            if (m_pAsyncOp) {
                                                                m_pAsyncOp->Wait();
                                                                delete m_pAsyncOp;
                                                            }

                                                            m_pAsyncOp = new AsyncOperation();
                                                            m_bShowProgressDialog = true;

                                                            m_pAsyncOp->Start(m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
                                                                try {
                                                                    size_t total = serviceNames.size();
                                                                    for (size_t i = 0; i < total; ++i) {
                                                                        const std::string& serviceName = serviceNames[i];
                                                                        float progress = static_cast<float>(i) / static_cast<float>(total);

                                                                        op->ReportProgress(progress, std::format("Deleting registry key for '{}'... ({}/{})", serviceName, i + 1, total));

                                                                        // Delete the registry key
                                                                        std::wstring wServiceName = utils::Utf8ToWide(serviceName);
                                                                        std::wstring keyPath = std::format(L"SYSTEM\\CurrentControlSet\\Services\\{}", wServiceName);

                                                                        LSTATUS result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str());
                                                                        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
                                                                            throw std::runtime_error(std::format("Failed to delete registry key for '{}': error {}", serviceName, result));
                                                                        }
                                                                    }

                                                                    op->ReportProgress(1.0f, std::format("Deleted registry keys for {} service(s) successfully", total));
                                                                    return true;
                                                                } catch (const std::exception& e) {
                                                                    spdlog::error("Failed to delete registry key: {}", e.what());
                                                                    return false;
                                                                }
                                                            });
                                                        }
                                                    }
                                                    break;

                                                case ServiceAction::Properties:
                                                    // Open properties dialog for all selected services
                                                    {
                                                        if (!m_selectedServices.empty()) {
                                                            // Cast away const - the dialog needs non-const pointers for editing
                                                            std::vector<ServiceInfo*> services;
                                                            for (const auto* svc : m_selectedServices) {
                                                                services.push_back(const_cast<ServiceInfo*>(svc));
                                                            }
                                                            m_pPropertiesDialog->Open(services);
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                        ImGui::EndPopup();
                                    }
                                } else {
                                    // Other columns: just display text
                                    ImGui::TextUnformatted(value.c_str());
                                }
                            }

                            // Pop text color if we pushed one
                            if (visualState != VisualState::Normal) {
                                ImGui::PopStyleColor();
                            }

                            ImGui::PopID();
                        }

                        ImGui::EndTable();

                        // Save table state periodically (throttled inside the method)
                        SaveServicesTableState();
                    }

                    // Status bar for Services view
                    ImGui::Separator();

                    // Calculate statistics
                    size_t visibleCount = filteredServices.size();
                    size_t totalCount = allServices.size();
                    size_t selectedCount = m_selectedServices.size();
                    size_t highlightedCount = 0;
                    size_t disabledCount = 0;
                    size_t filteredCount = totalCount - visibleCount;

                    for (const auto* service : allServices) {
                        VisualState state = m_pServicesController->GetVisualState(service);
                        if (state == VisualState::Highlighted) {
                            highlightedCount++;
                        } else if (state == VisualState::Disabled) {
                            disabledCount++;
                        }
                    }

                    // Display status bar with statistics in compartment style
                    ImGui::BeginGroup();
                    ImGui::Text("%zu visible", visibleCount);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%zu highlighted", highlightedCount);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%zu disabled", disabledCount);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%zu filtered", filteredCount);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%zu total", totalCount);
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::Text("%zu selected", selectedCount);
                    ImGui::EndGroup();
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

    // Render service properties dialog if open
    if (m_pPropertiesDialog && m_pPropertiesDialog->IsOpen()) {
        bool changesApplied = m_pPropertiesDialog->Render();
        if (changesApplied) {
            // Refresh services list to show updated data
            m_pServicesController->Refresh();
        }
    }

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
