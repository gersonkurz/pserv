#include "precomp.h"
#include "main_window.h"
#include "Resource.h"
#include <Config/settings.h>
#include <utils/win32_error.h>
#include <utils/string_utils.h>
#include <core/async_operation.h>
#include <core/data_controller_library.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace pserv {

	MainWindow::MainWindow() {
	}

	MainWindow::~MainWindow() {		
		CleanupImGui();
		CleanupDirectX();
		if (m_hWnd) {
			DestroyWindow(m_hWnd);
		}
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

		// Query Windows accent color for title bar
		DWORD accentColor = 0;
		BOOL opaque = FALSE;
		HRESULT hr = DwmGetColorizationColor(&accentColor, &opaque);
		if (SUCCEEDED(hr)) {
			m_accentColor = accentColor;
			spdlog::debug("Windows accent color: 0x{:08X}", accentColor);
		}
		else {
			// Default to a neutral blue if we can't get the accent color
			m_accentColor = RGB(0, 120, 212);
			spdlog::warn("Failed to get Windows accent color, using default");
		}

		// Load active tab from settings
		auto& appSettings = config::theSettings.application;
		m_activeTab = appSettings.activeView.get();
		spdlog::info("Loaded active tab from config: '{}'", m_activeTab);

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
			}
			else {
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
		case WM_ACTIVATE:
			if (pWindow) {
				pWindow->m_bWindowFocused = (LOWORD(wParam) != WA_INACTIVE);
			}
			return 0;

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
				pWindow->m_dispatchContext.m_bShowProgressDialog = false;
				if (pWindow->m_dispatchContext.m_pAsyncOp) {
					auto status = pWindow->m_dispatchContext.m_pAsyncOp->GetStatus();
					if (status == AsyncStatus::Completed) {
						spdlog::info("Async operation completed successfully");
						// Refresh controller to show updated state
						if (pWindow->m_dispatchContext.m_pController) {
							pWindow->m_dispatchContext.m_pController->Refresh();
						}
					}
					else if (status == AsyncStatus::Cancelled) {
						spdlog::info("Async operation was cancelled");
					}
					else if (status == AsyncStatus::Failed) {
						spdlog::error("Async operation failed: {}", pWindow->m_dispatchContext.m_pAsyncOp->GetErrorMessage());
					}
				}
			}
			return 0;

		case WM_DESTROY:
			if (pWindow) {
				pWindow->SaveWindowState();
				pWindow->SaveCurrentTableState(true); // Force save on window close
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
		}
		else {
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
			auto& style = ImGui::GetStyle();
			style.FontSizeBase = fontSize;
		}
		else {
			spdlog::error("No fonts available after rebuild!");
		}

		// Notify ImGui backends to update their font texture
		if (m_pDevice && m_pDeviceContext) {
			ImGui_ImplDX11_InvalidateDeviceObjects();
			ImGui_ImplDX11_CreateDeviceObjects();
		}
		else {
			spdlog::error("Device or context is null, cannot update font texture!");
		}
	}

	void MainWindow::RenderProgressDialog() {
		if (!m_dispatchContext.m_bShowProgressDialog || !m_dispatchContext.m_pAsyncOp) {
			return;
		}

		ImGui::OpenPopup("Operation in Progress");

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Operation in Progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			float progress = m_dispatchContext.m_pAsyncOp->GetProgress();
			std::string message = m_dispatchContext.m_pAsyncOp->GetProgressMessage();

			ImGui::Text("%s", message.c_str());
			ImGui::ProgressBar(progress, ImVec2(400, 0));
			ImGui::Text("%.1f%%", progress * 100.0f);

			ImGui::Separator();

			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_dispatchContext.m_pAsyncOp->RequestCancel();
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

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float titleBarHeight = static_cast<float>(GetSystemMetrics(SM_CYCAPTION));

		// Render custom title bar in a separate window at the very top
		{
			ImGui::SetNextWindowPos(viewport->Pos);  // Use Pos instead of WorkPos to start at 0,0
			ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titleBarHeight));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));  // No padding
			ImGuiWindowFlags titleBarFlags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoBringToFrontOnFocus;
			ImGui::Begin("TitleBar", nullptr, titleBarFlags);
			RenderTitleBar();
			ImGui::End();
			ImGui::PopStyleVar();
		}

		// Create main window below title bar for menu and content
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titleBarHeight));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - titleBarHeight));

		// Add some vertical padding for the menu bar
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));  // Add vertical padding

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_MenuBar;

		ImGui::Begin("MainWindow", nullptr, window_flags);

		// Render menu bar (ImGui positions this automatically at the top of this window)
		RenderMenuBar();

		ImGui::PopStyleVar();  // Pop FramePadding

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
			static bool firstFrame = true;

			for(const auto controller: m_Controllers.GetDataControllers()) {
				const std::string thisTabName = controller->GetControllerName();
				if (m_activeTab.empty())
				{
					// if we don't have an active tab yet, set it to the first controller
					m_activeTab = thisTabName;
				}

				// Set flags for tab selection
				ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
				if (firstFrame && (m_activeTab == thisTabName)) {
					flags |= ImGuiTabItemFlags_SetSelected;
				}
				// Handle pending tab switch from menu
				if (!m_pendingTabSwitch.empty() && (m_pendingTabSwitch == thisTabName)) {
					spdlog::debug("Applying pending tab switch to '{}' via ImGuiTabItemFlags_SetSelected", thisTabName);
					flags |= ImGuiTabItemFlags_SetSelected;
					m_pendingTabSwitch.clear();
				}

				if (ImGui::BeginTabItem(thisTabName.c_str(), nullptr, flags)) {
					// Detect if active tab changed (user clicked tab or menu triggered switch)
					// Skip change detection on first frame to allow ImGui to settle
					if (!firstFrame && (m_activeTab != thisTabName)) {
						spdlog::info("Active tab changing from '{}' to '{}'", m_activeTab, thisTabName);
						m_activeTab = thisTabName;
						m_pCurrentController = controller;
						auto& appSettings = config::theSettings.application;
						spdlog::debug("Setting appSettings.activeView to: {}", m_activeTab);
						appSettings.activeView.set(m_activeTab);
						if (m_pConfigBackend) {
							spdlog::debug("Saving config with activeView: {}", appSettings.activeView.get());
							appSettings.save(*m_pConfigBackend);
							spdlog::info("Active tab changed to '{}' and persisted to config", m_activeTab);
						}
						else {
							spdlog::warn("m_pConfigBackend is null, cannot persist active tab");
						}
					}

					// Always ensure controller is synced with visible tab
					if (m_pCurrentController != controller) {
						m_pCurrentController = controller;
						if (firstFrame) {
							spdlog::debug("First frame: syncing controller to active tab '{}'", thisTabName);
						}
					}

					RenderDataController(controller);

					ImGui::EndTabItem();
				}
			}

			firstFrame = false;
			ImGui::EndTabBar();
		}

		ImGui::End();

		// Render progress dialog if active
		RenderProgressDialog();

		if (m_pCurrentController)
		{
			m_pCurrentController->RenderPropertiesDialog();
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

	void MainWindow::RenderDataController(DataController* controller) {
		const auto& controllerName = controller->GetControllerName();

		// Lazy refresh if data is not loaded yet
		if (!controller->IsLoaded()) {
			try {
				controller->Refresh();
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to initial refresh {}: {}", controllerName, e.what());
			}
		}

		ImGui::Separator();
		{
			// Highlight refresh button if controller flagged that refresh is needed
			if (controller->NeedsRefresh()) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.0f, 1.0f));  // Orange
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.7f, 0.1f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));
			}

			const auto buttonName{ std::format("Refresh {}", controllerName) };
			if (ImGui::Button(buttonName.c_str())) {
				try {
					controller->Refresh();
					controller->ClearRefreshFlag();
				}
				catch (const std::exception& e) {
					spdlog::error("Failed to refresh {}: {}", controllerName, e.what());
				}
			}

			if (controller->NeedsRefresh()) {
				ImGui::PopStyleColor(3);
			}
		}

		// Filter input box
		ImGui::SameLine();
		ImGui::SetNextItemWidth(300.0f);

		{
			const auto label{ std::format("##filter_{}", controllerName) };
			const auto hint{ std::format("Filter {}...", controllerName) };
			ImGui::InputTextWithHint(label.c_str(), hint.c_str(), m_filterText, IM_ARRAYSIZE(m_filterText));
		}

		ImGui::Separator();

		// Reserve space for status bar at the bottom (one line of text + padding)
		float statusBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		float tableHeight = ImGui::GetContentRegionAvail().y - statusBarHeight;

		// Display data objects in a table
		const auto& columns = controller->GetColumns();

		// Declare these here so they're available for status bar later
		std::vector<const DataObject*> filteredDataObjects;
		const std::vector<DataObject*>* pAllDataObjects = nullptr;
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
		const auto tableName{ std::format("{}Table", controllerName) };
		if (ImGui::BeginTable(tableName.c_str(), static_cast<int>(columns.size()), flags, tableSize)) {

			m_pCurrentTable = ImGui::GetCurrentTable();

			const auto configSection = config::theSettings.getSectionFor(controllerName);

			// Load saved column widths from config
			std::vector<float> columnWidths;
			if (configSection)
			{
				const auto widthsStr = configSection->columnWidths.get();
				// Only log if this is the first time for this controller (avoid spam)
				static std::string lastLoadedController;
				if (lastLoadedController != controllerName) {
					spdlog::debug("Loading column widths for {}: {}", controllerName, widthsStr);
					lastLoadedController = controllerName;
				}
				
				std::istringstream iss{ widthsStr };
				std::string token;
				while (std::getline(iss, token, ',')) {
					try {
						columnWidths.push_back(std::stof(token));
					} catch (...) {
						columnWidths.push_back(100.0f); // Fallback for bad data
					}
				}
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
			// We check if the table needs sorting/ordering reset or if we just switched views
			static std::string lastOrderAppliedController;
			if (configSection && lastOrderAppliedController != controllerName) {
					ImGuiTable* table = ImGui::GetCurrentTable();
					const auto orderStr = configSection->columnOrder.get();
					spdlog::debug("Restoring column order for {}: {}", controllerName, orderStr);

					std::istringstream iss(orderStr);
					std::string token;
					std::vector<int> displayOrder;
					while (std::getline(iss, token, ',')) {
						try {
							displayOrder.push_back(std::stoi(token));
						} catch (...) {
							// Ignore malformed tokens
						}
					}

					// Apply DisplayOrder to columns and rebuild DisplayOrderToIndex
					if (displayOrder.size() == columns.size()) {
						// ... (debug logs omitted for brevity) ...

						for (size_t i = 0; i < displayOrder.size(); ++i) {
							if (i < static_cast<size_t>(table->ColumnsCount)) {
								table->Columns[static_cast<int>(i)].DisplayOrder = static_cast<ImGuiTableColumnIdx>(displayOrder[i]);
							}
						}

						// Rebuild DisplayOrderToIndex mapping
						for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
							table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = static_cast<ImGuiTableColumnIdx>(column_n);
						}
					}

					lastOrderAppliedController = controllerName;
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

						spdlog::info("[SORT] Controller '{}': Sorting by column {} ({})",
							controllerName, columnIndex, ascending ? "ascending" : "descending");
						controller->Sort(columnIndex, ascending);
						spdlog::info("[SORT] Controller '{}': Sort() completed", controllerName);
					}
					sortSpecs->SpecsDirty = false;
				}
			}

			// Get data objects AFTER sorting
			pAllDataObjects = &controller->GetDataObjects();

			// Filter data objects based on search text
			if (m_filterText[0] != '\0') {
				std::string lowerFilter = utils::ToLower(m_filterText);
				for (const auto* dataObject : *pAllDataObjects) {
					if (dataObject->MatchesFilter(lowerFilter)) {
						filteredDataObjects.push_back(dataObject);
					}
				}
			}
			else {
				// No filter - show all data Objects
				filteredDataObjects.assign(pAllDataObjects->begin(), pAllDataObjects->end());
			}

			// Lambda to render a single row (shared between clipper and non-clipper paths)
			auto renderRow = [&](const DataObject* dataObject) {
				ImGui::TableNextRow();
				ImGui::PushID(dataObject);

				// Get visual state from controller
				VisualState visualState = controller->GetVisualState(dataObject);

				// Pre-calculate highlight color for reuse
				ImVec4 highlightColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				highlightColor.x *= 0.6f; // Reduce red
				highlightColor.y *= 0.8f; // Reduce green
				highlightColor.z = 1.0f;  // Full blue

				// Apply text color based on visual state
				if (visualState == VisualState::Disabled) {
					// Gray out disabled items
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				}
				else if (visualState == VisualState::Highlighted) {
					// Highlight special items
					ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
				}

				m_dispatchContext.m_hWnd = m_hWnd;
				m_dispatchContext.m_pController = m_pCurrentController;

				// Display columns with context menu on right-click
				for (size_t i = 0; i < columns.size(); ++i) {
					ImGui::TableSetColumnIndex(static_cast<int>(i));
					std::string value = dataObject->GetProperty(static_cast<int>(i));

					// First column: use selectable to make row clickable
					if (i == 0) {
						// Check if this object is selected
						bool isSelected = std::find(m_dispatchContext.m_selectedObjects.begin(), m_dispatchContext.m_selectedObjects.end(), dataObject) != m_dispatchContext.m_selectedObjects.end();

						if (ImGui::Selectable(value.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
							// Handle selection logic
							ImGuiIO& io = ImGui::GetIO();

							if (io.KeyCtrl) {
								// Ctrl+Click: toggle selection
								if (isSelected) {
									m_dispatchContext.m_selectedObjects.erase(std::remove(m_dispatchContext.m_selectedObjects.begin(), m_dispatchContext.m_selectedObjects.end(), dataObject), m_dispatchContext.m_selectedObjects.end());
								}
								else {
									m_dispatchContext.m_selectedObjects.push_back(dataObject);
								}
								m_lastClickedObject = dataObject;
							}
							else if (io.KeyShift && m_lastClickedObject != nullptr) {
								// Shift+Click: range selection
								// Find the range between last clicked and current
								auto lastIt = std::find(filteredDataObjects.begin(), filteredDataObjects.end(), m_lastClickedObject);
								auto currentIt = std::find(filteredDataObjects.begin(), filteredDataObjects.end(), dataObject);

								if (lastIt != filteredDataObjects.end() && currentIt != filteredDataObjects.end()) {
									// Clear selection first
									m_dispatchContext.m_selectedObjects.clear();

									// Select range
									auto start = (lastIt < currentIt) ? lastIt : currentIt;
									auto end = (lastIt < currentIt) ? currentIt : lastIt;

									for (auto it = start; it <= end; ++it) {
										m_dispatchContext.m_selectedObjects.push_back(*it);
									}
								}
							}
							else {
								// Normal click: clear selection and select only this one
								m_dispatchContext.m_selectedObjects.clear();
								m_dispatchContext.m_selectedObjects.push_back(dataObject);
								m_lastClickedObject = dataObject;
							}
						}

						// Context menu for selected objects
						// BUG FIX: Temporarily pop row text color so context menu uses default color
						bool stylePopped = false;
						if (visualState != VisualState::Normal) {
							ImGui::PopStyleColor();
							stylePopped = true;
						}

						if (ImGui::BeginPopupContextItem()) {
							// If right-clicked on non-selected item, select only that one
							if (!isSelected) {
								m_dispatchContext.m_selectedObjects.clear();
								m_dispatchContext.m_selectedObjects.push_back(dataObject);
							}

							// Get all actions and filter to those available for this object
							auto allActions = controller->GetActions();
							for (const auto& action : allActions) {
								// Filter to context menu actions
								auto visibility = action->GetVisibility();
								if ((static_cast<int>(visibility) & static_cast<int>(ActionVisibility::ContextMenu)) == 0) {
									continue;
								}

								// Check availability for this object
								if (!action->IsAvailableFor(dataObject)) {
									continue;
								}

								// Handle separator
								if (action->IsSeparator()) {
									ImGui::Separator();
									continue;
								}

								// Show count if multiple objects selected
								std::string menuLabel = action->GetName();
								if (m_dispatchContext.m_selectedObjects.size() > 1) {
									menuLabel += std::format(" ({} selected)", m_dispatchContext.m_selectedObjects.size());
								}

								if (ImGui::MenuItem(menuLabel.c_str())) {
									action->Execute(m_dispatchContext);
								}
							}
							ImGui::EndPopup();
						}

						// Re-push row text color for subsequent columns
						if (stylePopped) {
							if (visualState == VisualState::Disabled) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
							}
							else if (visualState == VisualState::Highlighted) {
								ImGui::PushStyleColor(ImGuiCol_Text, highlightColor);
							}
						}
					}
					else {
						// Other columns: apply alignment based on column metadata
						ColumnAlignment alignment = columns[i].GetAlignment();
						if (alignment == ColumnAlignment::Right) {
							// Right-align: calculate text width and offset
							float textWidth = ImGui::CalcTextSize(value.c_str()).x;
							float columnWidth = ImGui::GetColumnWidth(static_cast<int>(i));
							float offset = columnWidth - textWidth - ImGui::GetStyle().ItemSpacing.x;
							if (offset > 0) {
								ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
							}
						}
						ImGui::TextUnformatted(value.c_str());
					}
				}

				// Pop text color if we pushed one
				if (visualState != VisualState::Normal) {
					ImGui::PopStyleColor();
				}

				ImGui::PopID();
			};

			// Display rows - use clipper for large datasets (>1000 items)
			const bool useClipper = filteredDataObjects.size() > 1000;

			if (useClipper) {
				// Virtual scrolling for large lists
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(filteredDataObjects.size()));
				while (clipper.Step()) {
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
						renderRow(filteredDataObjects[row]);
					}
				}
			} else {
				// Direct rendering for small lists
				for (const auto* dataObject : filteredDataObjects) {
					renderRow(dataObject);
				}
			}

			ImGui::EndTable();

			// Save table state periodically (throttled inside the method)
			SaveCurrentTableState();
		}

		// Status bar for objects view
		ImGui::Separator();

		// Calculate statistics
		size_t visibleCount = filteredDataObjects.size();
		size_t totalCount = pAllDataObjects ? pAllDataObjects->size() : 0;
		size_t selectedCount = m_dispatchContext.m_selectedObjects.size();
		size_t highlightedCount = 0;
		size_t disabledCount = 0;
		size_t filteredCount = totalCount - visibleCount;

		if (pAllDataObjects) {
			for (const auto* dataObject : *pAllDataObjects) {
				VisualState state = controller->GetVisualState(dataObject);
				if (state == VisualState::Highlighted) {
					highlightedCount++;
				}
				else if (state == VisualState::Disabled) {
					disabledCount++;
				}
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

	void MainWindow::SaveCurrentTableState(bool force) {

		if (!m_pConfigBackend) {
			spdlog::warn("Cannot save current table state: pConfigBackend is null");
			return;
		}

		if (!m_pCurrentTable) {
			spdlog::debug("Current tabe pointer is null, skipping save");
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

		auto configSection = config::theSettings.getSectionFor(m_pCurrentController->GetControllerName());
		if (!configSection)
		{
			spdlog::debug("getSectionFor({}) pointer is null, skipping save", m_pCurrentController->GetControllerName());
			return;
		}

		// Build current state strings
		std::ostringstream widthsStream;
		for (int i = 0; i < m_pCurrentTable->ColumnsCount; ++i) {
			if (i > 0) widthsStream << ",";
			widthsStream << m_pCurrentTable->Columns[i].WidthGiven;
		}
		std::string currentWidths = widthsStream.str();

		std::ostringstream orderStream;
		for (int i = 0; i < m_pCurrentTable->ColumnsCount; ++i) {
			if (i > 0) orderStream << ",";
			orderStream << static_cast<int>(m_pCurrentTable->Columns[i].DisplayOrder);
		}
		std::string currentOrder = orderStream.str();

		// Check if state actually changed
		static std::string lastWidths;
		static std::string lastOrder;
		static std::string lastController;
		std::string currentController = m_pCurrentController->GetControllerName();

		if (!force && currentController == lastController &&
		    currentWidths == lastWidths && currentOrder == lastOrder) {
			spdlog::trace("Skipping save: table state unchanged");
			return;
		}

		// Update cache
		lastController = currentController;
		lastWidths = currentWidths;
		lastOrder = currentOrder;

		spdlog::debug("Proceeding with save");
		spdlog::debug("Column widths: {}", currentWidths);
		spdlog::debug("Column order: {}", currentOrder);

		// Save to config
		configSection->columnWidths.set(currentWidths);
		configSection->columnOrder.set(currentOrder);
		configSection->save(*m_pConfigBackend);

		spdlog::info("Current table state saved: widths={}, order={}", currentWidths, currentOrder);
	}

	bool MainWindow::IsWindowMaximized() const {
		if (!m_hWnd) return false;

		WINDOWPLACEMENT wp{};
		wp.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(m_hWnd, &wp)) {
			return wp.showCmd == SW_SHOWMAXIMIZED;
		}
		return false;
	}

	void MainWindow::RenderTitleBar() {
		const float titleBarHeight = static_cast<float>(GetSystemMetrics(SM_CYCAPTION));
		const float buttonWidth = 46.0f;
		const float buttonHeight = titleBarHeight;

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 titleBarMin = ImGui::GetCursorScreenPos();
		ImVec2 titleBarMax = ImVec2(titleBarMin.x + ImGui::GetContentRegionAvail().x, titleBarMin.y + titleBarHeight);

		// Draw title bar background with accent color when focused
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec4 titleBarColor;

		if (m_bWindowFocused) {
			// Use Windows accent color when focused
			float r = GetRValue(m_accentColor) / 255.0f;
			float g = GetGValue(m_accentColor) / 255.0f;
			float b = GetBValue(m_accentColor) / 255.0f;
			titleBarColor = ImVec4(r, g, b, 1.0f);
		}
		else {
			// Use neutral gray when unfocused
			titleBarColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
		}

		drawList->AddRectFilled(titleBarMin, titleBarMax, ImGui::ColorConvertFloat4ToU32(titleBarColor));

		// Handle window dragging
		ImVec2 mousePos = io.MousePos;
		bool mouseInTitleBar = mousePos.x >= titleBarMin.x && mousePos.x <= titleBarMax.x &&
			mousePos.y >= titleBarMin.y && mousePos.y <= titleBarMax.y;

		// Check if mouse is NOT over window control buttons (rightmost 3 buttons)
		float buttonsAreaStart = titleBarMax.x - (buttonWidth * 3);
		bool mouseInButtons = mousePos.x >= buttonsAreaStart && mousePos.x <= titleBarMax.x &&
			mousePos.y >= titleBarMin.y && mousePos.y <= titleBarMax.y;

		if (mouseInTitleBar && !mouseInButtons && ImGui::IsMouseClicked(0)) {
			// Send WM_NCLBUTTONDOWN to enable native window dragging and snapping
			ReleaseCapture();
			SendMessageW(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		}

		// Draw window title on the left
		ImGui::SetCursorScreenPos(ImVec2(titleBarMin.x + 10.0f, titleBarMin.y + (titleBarHeight - ImGui::GetTextLineHeight()) * 0.5f));
		ImGui::Text("pserv5");

		// Window control buttons (right side)
		ImVec2 buttonPos = ImVec2(titleBarMax.x - buttonWidth * 3, titleBarMin.y);

		// Minimize button - using horizontal line symbol
		ImGui::SetCursorScreenPos(buttonPos);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

		if (ImGui::Button("\xE2\x80\x94##minimize", ImVec2(buttonWidth, buttonHeight))) {  // EM DASH (UTF-8)
			ShowWindow(m_hWnd, SW_MINIMIZE);
		}

		ImGui::PopStyleColor(3);

		// Maximize/Restore button - using square symbols
		buttonPos.x += buttonWidth;
		ImGui::SetCursorScreenPos(buttonPos);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

		bool isMaximized = IsWindowMaximized();
		const char* maxButtonLabel = isMaximized ? "\xE2\x9D\x8F##restore" : "\xE2\x96\xA1##maximize";  // SHADOWED SQUARE / WHITE SQUARE (UTF-8)

		if (ImGui::Button(maxButtonLabel, ImVec2(buttonWidth, buttonHeight))) {
			ShowWindow(m_hWnd, isMaximized ? SW_RESTORE : SW_MAXIMIZE);
		}

		ImGui::PopStyleColor(3);

		// Close button - using X symbol with red hover
		buttonPos.x += buttonWidth;
		ImGui::SetCursorScreenPos(buttonPos);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));  // White text

		if (ImGui::Button("X##close", ImVec2(buttonWidth, buttonHeight))) {
			DestroyWindow(m_hWnd);
		}

		ImGui::PopStyleColor(4);
	}

	void MainWindow::RenderMenuBar() {
		static bool showAboutDialog = false;

		if (ImGui::BeginMenuBar()) {
			// File menu
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Refresh", "F5")) {
					if (m_pCurrentController) {
						try {
							m_pCurrentController->Refresh();
						}
						catch (const std::exception& e) {
							spdlog::error("Failed to refresh: {}", e.what());
						}
					}
				}

				if (ImGui::MenuItem("Export to XML...", nullptr, false, false)) {
					// TBD in future milestone
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit", "Alt+F4")) {
					DestroyWindow(m_hWnd);
				}

				ImGui::EndMenu();
			}

			// View menu
			if (ImGui::BeginMenu("View")) {

				int shortcutIndex = 1;
				for (const auto controller : m_Controllers.GetDataControllers()) {
					const std::string tabName = controller->GetControllerName();
					bool selected = (m_activeTab == tabName);
					const auto shortcut = std::format("Ctrl+{}", shortcutIndex++);
					if (ImGui::MenuItem(tabName.c_str(), shortcut.c_str(), selected)) {
						// Request tab switch on next frame
						spdlog::debug("View menu: requesting tab switch to '{}'", tabName);
						m_pendingTabSwitch = tabName;
					}
				}

				ImGui::EndMenu();
			}

			// Tools menu
			if (ImGui::BeginMenu("Tools")) {
				if (ImGui::MenuItem("Options...", nullptr, false, false)) {
					// TBD in future milestone
				}

				if (ImGui::MenuItem("Connect to Remote Machine...", nullptr, false, false)) {
					// TBD in future milestone
				}

				ImGui::EndMenu();
			}

			// Themes menu
			if (ImGui::BeginMenu("Themes")) {
				auto& appSettings = config::theSettings.application;
				std::string currentTheme = appSettings.theme.get();

				if (ImGui::MenuItem("Dark", nullptr, currentTheme == "Dark")) {
					appSettings.theme.set("Dark");
					if (m_pConfigBackend) {
						appSettings.save(*m_pConfigBackend);
					}
					ImGui::StyleColorsDark();
				}

				if (ImGui::MenuItem("Light", nullptr, currentTheme == "Light")) {
					appSettings.theme.set("Light");
					if (m_pConfigBackend) {
						appSettings.save(*m_pConfigBackend);
					}
					ImGui::StyleColorsLight();
				}

				if (ImGui::MenuItem("Classic", nullptr, currentTheme == "Classic")) {
					appSettings.theme.set("Classic");
					if (m_pConfigBackend) {
						appSettings.save(*m_pConfigBackend);
					}
					ImGui::StyleColorsClassic();
				}

				ImGui::EndMenu();
			}

			// Help menu
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("About pserv5...")) {
					showAboutDialog = true;
				}

				if (ImGui::MenuItem("Documentation")) {
					ShellExecuteW(nullptr, L"open", L"http://p-nand-q.com/download/pserv_cpl/index.html", nullptr, nullptr, SW_SHOWNORMAL);
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Check for Updates...", nullptr, false, false)) {
					// TBD in future milestone
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// About dialog
		if (showAboutDialog) {
			ImGui::OpenPopup("About pserv5");
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("About pserv5", &showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("pserv5");
			ImGui::Separator();
			ImGui::Text("Version 5.0.0");
			ImGui::Text("A modern Windows management utility");
			ImGui::Spacing();
			ImGui::Text("Copyright (c) 2025");
			ImGui::Text("http://p-nand-q.com");
			ImGui::Spacing();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				showAboutDialog = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

} // namespace pserv
