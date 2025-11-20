#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_action.h>
#include <models/service_info.h>
#include <windows_api/service_manager.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>
#include <imgui.h>

namespace pserv {

// Forward declare to avoid circular dependency
class ServicesDataController;

namespace {

// Helper function to get ServiceInfo from DataObject
inline const ServiceInfo* GetServiceInfo(const DataObject* obj) {
	return static_cast<const ServiceInfo*>(obj);
}

// ============================================================================
// Service Lifecycle Actions
// ============================================================================

class ServiceStartAction final : public DataAction {
public:
	ServiceStartAction() : DataAction{"Start", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		return GetServiceInfo(obj)->GetCurrentState() == SERVICE_STOPPED;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Start {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float baseProgress = static_cast<float>(i) / static_cast<float>(total);
					float progressRange = 1.0f / static_cast<float>(total);

					op->ReportProgress(baseProgress, std::format("Starting service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::StartServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
						op->ReportProgress(baseProgress + progress * progressRange, message);
					});
				}

				op->ReportProgress(1.0f, std::format("Started {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to start service: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceStopAction final : public DataAction {
public:
	ServiceStopAction() : DataAction{"Stop", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		DWORD state = GetServiceInfo(obj)->GetCurrentState();
		return (state == SERVICE_RUNNING || state == SERVICE_PAUSED);
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Stop {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float baseProgress = static_cast<float>(i) / static_cast<float>(total);
					float progressRange = 1.0f / static_cast<float>(total);

					op->ReportProgress(baseProgress, std::format("Stopping service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::StopServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
						op->ReportProgress(baseProgress + progress * progressRange, message);
					});
				}

				op->ReportProgress(1.0f, std::format("Stopped {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to stop service: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceRestartAction final : public DataAction {
public:
	ServiceRestartAction() : DataAction{"Restart", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		return GetServiceInfo(obj)->GetCurrentState() == SERVICE_RUNNING;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Restart {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float baseProgress = static_cast<float>(i) / static_cast<float>(total);
					float progressRange = 1.0f / static_cast<float>(total);

					op->ReportProgress(baseProgress, std::format("Restarting service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::RestartServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
						op->ReportProgress(baseProgress + progress * progressRange, message);
					});
				}

				op->ReportProgress(1.0f, std::format("Restarted {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to restart service: {}", e.what());
				return false;
			}
		});
	}
};

class ServicePauseAction final : public DataAction {
public:
	ServicePauseAction() : DataAction{"Pause", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		const auto* svc = GetServiceInfo(obj);
		return (svc->GetCurrentState() == SERVICE_RUNNING) &&
		       (svc->GetControlsAccepted() & SERVICE_ACCEPT_PAUSE_CONTINUE);
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Pause {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float baseProgress = static_cast<float>(i) / static_cast<float>(total);
					float progressRange = 1.0f / static_cast<float>(total);

					op->ReportProgress(baseProgress, std::format("Pausing service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::PauseServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
						op->ReportProgress(baseProgress + progress * progressRange, message);
					});
				}

				op->ReportProgress(1.0f, std::format("Paused {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to pause service: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceResumeAction final : public DataAction {
public:
	ServiceResumeAction() : DataAction{"Resume", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		return GetServiceInfo(obj)->GetCurrentState() == SERVICE_PAUSED;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Resume {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float baseProgress = static_cast<float>(i) / static_cast<float>(total);
					float progressRange = 1.0f / static_cast<float>(total);

					op->ReportProgress(baseProgress, std::format("Resuming service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::ResumeServiceByName(serviceName, [op, baseProgress, progressRange](float progress, std::string message) {
						op->ReportProgress(baseProgress + progress * progressRange, message);
					});
				}

				op->ReportProgress(1.0f, std::format("Resumed {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to resume service: {}", e.what());
				return false;
			}
		});
	}
};

// ============================================================================
// Copy Actions
// ============================================================================

class ServiceCopyNameAction final : public DataAction {
public:
	ServiceCopyNameAction() : DataAction{"Copy Name", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string result;
		for (const auto* svc : ctx.m_selectedObjects) {
			if (!result.empty()) result += "\n";
			result += GetServiceInfo(svc)->GetName();
		}
		ImGui::SetClipboardText(result.c_str());
		spdlog::debug("Copied {} service name(s) to clipboard", ctx.m_selectedObjects.size());
	}
};

class ServiceCopyDisplayNameAction final : public DataAction {
public:
	ServiceCopyDisplayNameAction() : DataAction{"Copy Display Name", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string result;
		for (const auto* svc : ctx.m_selectedObjects) {
			if (!result.empty()) result += "\n";
			result += GetServiceInfo(svc)->GetDisplayName();
		}
		ImGui::SetClipboardText(result.c_str());
		spdlog::debug("Copied {} service display name(s) to clipboard", ctx.m_selectedObjects.size());
	}
};

class ServiceCopyBinaryPathAction final : public DataAction {
public:
	ServiceCopyBinaryPathAction() : DataAction{"Copy Binary Path", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string result;
		for (const auto* svc : ctx.m_selectedObjects) {
			if (!result.empty()) result += "\n";
			result += GetServiceInfo(svc)->GetBinaryPathName();
		}
		ImGui::SetClipboardText(result.c_str());
		spdlog::debug("Copied {} service binary path(s) to clipboard", ctx.m_selectedObjects.size());
	}
};

// ============================================================================
// Startup Type Configuration Actions
// ============================================================================

class ServiceSetStartupAutomaticAction final : public DataAction {
public:
	ServiceSetStartupAutomaticAction() : DataAction{"Set Startup: Automatic", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Set startup type to Automatic for {} service(s)",
			serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float progress = static_cast<float>(i) / static_cast<float>(total);

					op->ReportProgress(progress, std::format("Setting startup type for '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::ChangeServiceStartType(serviceName, SERVICE_AUTO_START);
				}

				op->ReportProgress(1.0f, std::format("Set startup type to Automatic for {} service(s)", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to change startup type: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceSetStartupManualAction final : public DataAction {
public:
	ServiceSetStartupManualAction() : DataAction{"Set Startup: Manual", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Set startup type to Manual for {} service(s)",
			serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float progress = static_cast<float>(i) / static_cast<float>(total);

					op->ReportProgress(progress, std::format("Setting startup type for '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::ChangeServiceStartType(serviceName, SERVICE_DEMAND_START);
				}

				op->ReportProgress(1.0f, std::format("Set startup type to Manual for {} service(s)", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to change startup type: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceSetStartupDisabledAction final : public DataAction {
public:
	ServiceSetStartupDisabledAction() : DataAction{"Set Startup: Disabled", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		spdlog::info("Starting async operation: Set startup type to Disabled for {} service(s)",
			serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float progress = static_cast<float>(i) / static_cast<float>(total);

					op->ReportProgress(progress, std::format("Setting startup type for '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::ChangeServiceStartType(serviceName, SERVICE_DISABLED);
				}

				op->ReportProgress(1.0f, std::format("Set startup type to Disabled for {} service(s)", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to change startup type: {}", e.what());
				return false;
			}
		});
	}
};

// ============================================================================
// System Integration Actions
// ============================================================================

class ServiceOpenInRegistryEditorAction final : public DataAction {
public:
	ServiceOpenInRegistryEditorAction() : DataAction{"Open in Registry Editor", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string serviceName = GetServiceInfo(ctx.m_selectedObjects[0])->GetName();
		std::string regPath = std::format("SYSTEM\\CurrentControlSet\\Services\\{}", serviceName);

		HKEY hKey;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
			0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
			std::wstring fullPath = utils::Utf8ToWide(std::format("HKEY_LOCAL_MACHINE\\{}", regPath));
			RegSetValueExW(hKey, L"LastKey", 0, REG_SZ, (const BYTE*)fullPath.c_str(),
				(DWORD)(fullPath.length() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}

		spdlog::info("Opening registry editor for: {}", serviceName);
		ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOW);
	}
};

class ServiceOpenInExplorerAction final : public DataAction {
public:
	ServiceOpenInExplorerAction() : DataAction{"Open in Explorer", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string installLocation = GetServiceInfo(ctx.m_selectedObjects[0])->GetInstallLocation();
		if (!installLocation.empty()) {
			spdlog::info("Opening explorer: {}", installLocation);
			std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
			ShellExecuteW(NULL, L"open", wInstallLocation.c_str(), NULL, NULL, SW_SHOW);
		}
		else {
			spdlog::warn("No install location available for this service");
		}
	}
};

class ServiceOpenTerminalHereAction final : public DataAction {
public:
	ServiceOpenTerminalHereAction() : DataAction{"Open Terminal Here", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::string installLocation = GetServiceInfo(ctx.m_selectedObjects[0])->GetInstallLocation();
		if (!installLocation.empty()) {
			spdlog::info("Opening terminal in: {}", installLocation);
			std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
			ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, wInstallLocation.c_str(), SW_SHOW);
		}
		else {
			spdlog::warn("No install location available for this service");
		}
	}
};

// ============================================================================
// Dangerous Actions
// ============================================================================

class ServiceUninstallAction final : public DataAction {
public:
	ServiceUninstallAction() : DataAction{"Uninstall Service", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	bool IsDestructive() const override { return true; }

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		// Show confirmation dialog
		std::string confirmMsg;
		if (serviceNames.size() == 1) {
			confirmMsg = std::format("Are you sure you want to delete the service '{}'?\n\nThis will remove the service from the system.",
				serviceNames[0]);
		}
		else {
			confirmMsg = std::format("Are you sure you want to delete {} services?\n\nThis will remove all selected services from the system.",
				serviceNames.size());
		}

		int result = MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Service Deletion", MB_YESNO | MB_ICONWARNING);
		if (result != IDYES) return;

		spdlog::info("Starting async operation: Delete {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float progress = static_cast<float>(i) / static_cast<float>(total);

					op->ReportProgress(progress, std::format("Deleting service '{}'... ({}/{})",
						serviceName, i + 1, total));

					ServiceManager::DeleteService(serviceName);
				}

				op->ReportProgress(1.0f, std::format("Deleted {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to delete service: {}", e.what());
				return false;
			}
		});
	}
};

class ServiceDeleteRegistryKeyAction final : public DataAction {
public:
	ServiceDeleteRegistryKeyAction() : DataAction{"Delete Registry Key", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	bool IsDestructive() const override { return true; }

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<std::string> serviceNames;
		for (const auto* svc : ctx.m_selectedObjects) {
			serviceNames.push_back(GetServiceInfo(svc)->GetName());
		}

		// Show confirmation dialog
		std::string confirmMsg;
		if (serviceNames.size() == 1) {
			confirmMsg = std::format("Are you sure you want to delete the registry key for service '{}'?\n\n"
				"This will remove: HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\{}\n\n"
				"This is typically used to clean up orphaned service registry entries.",
				serviceNames[0], serviceNames[0]);
		}
		else {
			confirmMsg = std::format("Are you sure you want to delete the registry keys for {} services?\n\n"
				"This will remove the registry entries under HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\",
				serviceNames.size());
		}

		int result = MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Registry Key Deletion", MB_YESNO | MB_ICONWARNING);
		if (result != IDYES) return;

		spdlog::info("Starting async operation: Delete registry keys for {} service(s)", serviceNames.size());

		if (ctx.m_pAsyncOp) {
			ctx.m_pAsyncOp->Wait();
			delete ctx.m_pAsyncOp;
		}

		ctx.m_pAsyncOp = new AsyncOperation();
		ctx.m_bShowProgressDialog = true;

		ctx.m_pAsyncOp->Start(ctx.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
			try {
				size_t total = serviceNames.size();
				for (size_t i = 0; i < total; ++i) {
					const std::string& serviceName = serviceNames[i];
					float progress = static_cast<float>(i) / static_cast<float>(total);

					op->ReportProgress(progress, std::format("Deleting registry key for '{}'... ({}/{})",
						serviceName, i + 1, total));

					std::wstring wServiceName = utils::Utf8ToWide(serviceName);
					std::wstring keyPath = std::format(L"SYSTEM\\CurrentControlSet\\Services\\{}", wServiceName);

					LSTATUS result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str());
					if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
						throw std::runtime_error(std::format("Failed to delete registry key for '{}': error {}",
							serviceName, result));
					}
				}

				op->ReportProgress(1.0f, std::format("Deleted registry keys for {} service(s) successfully", total));
				return true;
			}
			catch (const std::exception& e) {
				spdlog::error("Failed to delete registry key: {}", e.what());
				return false;
			}
		});
	}
};

// ============================================================================
// Properties Dialog Action
// ============================================================================

class ServicePropertiesAction final : public DataAction {
public:
	ServicePropertiesAction() : DataAction{"Properties...", ActionVisibility::Both} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		// Will be implemented when we integrate with controllers
		// For now, this is a stub
		spdlog::info("Properties action triggered for {} service(s)", ctx.m_selectedObjects.size());
	}
};

} // anonymous namespace

// ============================================================================
// Factory Function
// ============================================================================

std::vector<std::shared_ptr<DataAction>> CreateServiceActions() {
	return {
		std::make_shared<ServicePropertiesAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ServiceStartAction>(),
		std::make_shared<ServiceStopAction>(),
		std::make_shared<ServiceRestartAction>(),
		std::make_shared<ServicePauseAction>(),
		std::make_shared<ServiceResumeAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ServiceCopyNameAction>(),
		std::make_shared<ServiceCopyDisplayNameAction>(),
		std::make_shared<ServiceCopyBinaryPathAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ServiceSetStartupAutomaticAction>(),
		std::make_shared<ServiceSetStartupManualAction>(),
		std::make_shared<ServiceSetStartupDisabledAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ServiceOpenInRegistryEditorAction>(),
		std::make_shared<ServiceOpenInExplorerAction>(),
		std::make_shared<ServiceOpenTerminalHereAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ServiceUninstallAction>(),
		std::make_shared<ServiceDeleteRegistryKeyAction>()
	};
}

} // namespace pserv
