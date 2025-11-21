#include "precomp.h"
#include <controllers/services_data_controller.h>
#include <actions/service_actions.h>
#include <windows_api/service_manager.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>

#include <imgui.h>

namespace pserv {

	ServicesDataController::ServicesDataController()
		: DataController{"Services", "Service", {
			{"Display Name", "DisplayName", ColumnDataType::String},
			{"Name", "Name", ColumnDataType::String},
			{"Status", "Status", ColumnDataType::String},
			{"Start Type", "StartType", ColumnDataType::String},
			{"Process ID", "ProcessId", ColumnDataType::UnsignedInteger},
			{"Service Type", "ServiceType", ColumnDataType::String},
			{"Binary Path Name", "BinaryPathName", ColumnDataType::String},
			{"Description", "Description", ColumnDataType::String},
			{"User", "User", ColumnDataType::String},
			{"Load Order Group", "LoadOrderGroup", ColumnDataType::String},
			{"Error Control", "ErrorControl", ColumnDataType::String},
			{"Tag ID", "TagId", ColumnDataType::UnsignedInteger},
			{"Win32 Exit Code", "Win32ExitCode", ColumnDataType::UnsignedInteger},
			{"Service Specific Exit Code", "ServiceSpecificExitCode", ColumnDataType::UnsignedInteger},
			{"Check Point", "CheckPoint", ColumnDataType::UnsignedInteger},
			{"Wait Hint", "WaitHint", ColumnDataType::UnsignedInteger},
			{"Service Flags", "ServiceFlags", ColumnDataType::UnsignedInteger},
			{"Controls Accepted", "ControlsAccepted", ColumnDataType::String}
		} }
		, m_serviceType{SERVICE_WIN32}
	{
	}

	ServicesDataController::ServicesDataController(DWORD serviceType, const char* viewName, const char* itemName)
		: DataController{viewName, itemName, {
			{"Display Name", "DisplayName", ColumnDataType::String},
			{"Name", "Name", ColumnDataType::String},
			{"Status", "Status", ColumnDataType::String},
			{"Start Type", "StartType", ColumnDataType::String},
			{"Process ID", "ProcessId", ColumnDataType::UnsignedInteger},
			{"Service Type", "ServiceType", ColumnDataType::String},
			{"Binary Path Name", "BinaryPathName", ColumnDataType::String},
			{"Description", "Description", ColumnDataType::String},
			{"User", "User", ColumnDataType::String},
			{"Load Order Group", "LoadOrderGroup", ColumnDataType::String},
			{"Error Control", "ErrorControl", ColumnDataType::String},
			{"Tag ID", "TagId", ColumnDataType::UnsignedInteger},
			{"Win32 Exit Code", "Win32ExitCode", ColumnDataType::UnsignedInteger},
			{"Service Specific Exit Code", "ServiceSpecificExitCode", ColumnDataType::UnsignedInteger},
			{"Check Point", "CheckPoint", ColumnDataType::UnsignedInteger},
			{"Wait Hint", "WaitHint", ColumnDataType::UnsignedInteger},
			{"Service Flags", "ServiceFlags", ColumnDataType::UnsignedInteger},
			{"Controls Accepted", "ControlsAccepted", ColumnDataType::String}
		} }
		, m_serviceType{serviceType}
	{
	}

	void ServicesDataController::Refresh() {
		spdlog::info("Refreshing services...");

		// Clear existing services
		Clear();

		try {
			// Enumerate services with the configured service type filter
			ServiceManager sm;
			m_objects = sm.EnumerateServices(m_serviceType);

			spdlog::info("Successfully refreshed {} services", m_objects.size());

			// Re-apply last sort order if any
			if (m_lastSortColumn >= 0) {
				Sort(m_lastSortColumn, m_lastSortAscending);
			}

			m_bLoaded = true;
		}
		catch (const std::exception& e) {
			spdlog::error("Failed to refresh services: {}", e.what());
			throw;
		}
	}

	std::vector<const DataAction*> ServicesDataController::GetActions(const DataObject* dataObject) const {
		
		const auto service = dynamic_cast<const ServiceInfo*>(dataObject);
		const auto currentState = service->GetCurrentState();
		const auto controlsAccepted = service->GetControlsAccepted();
		return CreateServiceActions(currentState, controlsAccepted);
	}

	VisualState ServicesDataController::GetVisualState(const DataObject* dataObject) const {
		if (!dataObject) {
			return VisualState::Normal;
		}

		const auto service = dynamic_cast<const ServiceInfo*>(dataObject);

		// Check if service is disabled
		DWORD startType = service->GetStartType();
		if (startType == SERVICE_DISABLED) {
			return VisualState::Disabled;
		}

		// Check if service is running
		DWORD currentState = service->GetCurrentState();
		if (currentState == SERVICE_RUNNING) {
			return VisualState::Highlighted;
		}

		return VisualState::Normal;
	}
	/*
	void ServicesDataController::DispatchAction(int action, DataActionDispatchContext& dispatchContext)
	{
		// Handle action
		switch (static_cast<ServiceAction>(action)) {
		case ServiceAction::CopyName:
			// Copy names of all selected services
		{
			std::string result;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				if (!result.empty()) result += "\n";
				result += static_cast<const ServiceInfo*>(svc)->GetName();
			}
			// TBD: should really be a callback action
			ImGui::SetClipboardText(result.c_str());
			spdlog::debug("Copied {} object name(s) to clipboard", dispatchContext.m_selectedObjects.size());
		}
		break;

		case ServiceAction::CopyDisplayName:
			// Copy display names of all selected services
		{
			std::string result;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				if (!result.empty()) result += "\n";
				result += static_cast<const ServiceInfo*>(svc)->GetDisplayName();
			}
			ImGui::SetClipboardText(result.c_str());
			spdlog::debug("Copied {} object display name(s) to clipboard", dispatchContext.m_selectedObjects.size());
		}
		break;

		case ServiceAction::CopyBinaryPath:
			// Copy binary paths of all selected services
		{
			std::string result;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				if (!result.empty()) result += "\n";
				result += static_cast<const ServiceInfo*>(svc)->GetBinaryPathName();
			}
			ImGui::SetClipboardText(result.c_str());
			spdlog::debug("Copied {} data object binary path(s) to clipboard", dispatchContext.m_selectedObjects.size());
		}
		break;

		case ServiceAction::Start:
			// Start service(s) asynchronously
		{
			// Copy selected services list for async operation
			std::vector<std::string> serviceNames;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Start {} service(s)", serviceNames.size());

			// Clean up previous async operation if any
			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			// Create new async operation
			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			// Start the service(s) in a worker thread
			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
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
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Stop {} service(s)", serviceNames.size());

			// Clean up previous async operation if any
			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			// Create new async operation
			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			// Stop the service(s) in a worker thread
			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
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
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Pause {} service(s)", serviceNames.size());

			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
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
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Resume {} service(s)", serviceNames.size());

			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
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
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Restart {} service(s)", serviceNames.size());

			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
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
			if (static_cast<ServiceAction>(action) == ServiceAction::SetStartupAutomatic) {
				startType = SERVICE_AUTO_START;
				startTypeName = "Automatic";
			}
			else if (static_cast<ServiceAction>(action) == ServiceAction::SetStartupManual) {
				startType = SERVICE_DEMAND_START;
				startTypeName = "Manual";
			}
			else {
				startType = SERVICE_DISABLED;
				startTypeName = "Disabled";
			}

			// Copy selected services list for async operation
			std::vector<std::string> serviceNames;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			spdlog::info("Starting async operation: Set startup type to {} for {} service(s)",
				startTypeName, serviceNames.size());

			if (dispatchContext.m_pAsyncOp) {
				dispatchContext.m_pAsyncOp->Wait();
				delete dispatchContext.m_pAsyncOp;
				dispatchContext.m_pAsyncOp = nullptr;
			}

			dispatchContext.m_pAsyncOp = new AsyncOperation();
			dispatchContext.m_bShowProgressDialog = true;

			dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames, startType, startTypeName](AsyncOperation* op) -> bool {
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
				}
				catch (const std::exception& e) {
					spdlog::error("Failed to change startup type: {}", e.what());
					return false;
				}
				});
		}
		break;

		case ServiceAction::OpenInRegistryEditor:
			// Open registry editor and navigate to service key
		{
			std::string serviceName = static_cast<const ServiceInfo*>(dispatchContext.m_selectedObjects[0])->GetName();
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
			std::string installLocation = static_cast<const ServiceInfo*>(dispatchContext.m_selectedObjects[0])->GetInstallLocation();
			if (!installLocation.empty()) {
				spdlog::info("Opening explorer: {}", installLocation);
				std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
				ShellExecuteW(NULL, L"open", wInstallLocation.c_str(), NULL, NULL, SW_SHOW);
			}
			else {
				spdlog::warn("No install location available for this service");
			}
		}
		break;

		case ServiceAction::OpenTerminalHere:
			// Open terminal in first selected service's install location
		{
			std::string installLocation = static_cast<const ServiceInfo*>(dispatchContext.m_selectedObjects[0])->GetInstallLocation();
			if (!installLocation.empty()) {
				spdlog::info("Opening terminal in: {}", installLocation);
				std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
				ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, wInstallLocation.c_str(), SW_SHOW);
			}
			else {
				spdlog::warn("No install location available for this service");
			}
		}
		break;

		case ServiceAction::UninstallService:
			// Delete selected service(s) with confirmation
		{
			// Copy selected services list
			std::vector<std::string> serviceNames;
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			// Show confirmation dialog
			std::string confirmMsg;
			if (serviceNames.size() == 1) {
				confirmMsg = std::format("Are you sure you want to delete the service '{}'?\n\nThis will remove the service from the system.", serviceNames[0]);
			}
			else {
				confirmMsg = std::format("Are you sure you want to delete {} services?\n\nThis will remove all selected services from the system.", serviceNames.size());
			}

			int result = MessageBoxA(dispatchContext.m_hWnd, confirmMsg.c_str(), "Confirm Service Deletion", MB_YESNO | MB_ICONWARNING);
			if (result == IDYES) {
				spdlog::info("Starting async operation: Delete {} service(s)", serviceNames.size());

				if (dispatchContext.m_pAsyncOp) {
					dispatchContext.m_pAsyncOp->Wait();
					delete dispatchContext.m_pAsyncOp;
					dispatchContext.m_pAsyncOp = nullptr;
				}

				dispatchContext.m_pAsyncOp = new AsyncOperation();
				dispatchContext.m_bShowProgressDialog = true;

				dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
					}
					catch (const std::exception& e) {
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
			for (const auto* svc : dispatchContext.m_selectedObjects) {
				serviceNames.push_back(static_cast<const ServiceInfo*>(svc)->GetName());
			}

			// Show confirmation dialog
			std::string confirmMsg;
			if (serviceNames.size() == 1) {
				confirmMsg = std::format("Are you sure you want to delete the registry key for service '{}'?\n\nThis will remove: HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\{}\n\nThis is typically used to clean up orphaned service registry entries.", serviceNames[0], serviceNames[0]);
			}
			else {
				confirmMsg = std::format("Are you sure you want to delete the registry keys for {} services?\n\nThis will remove the registry entries under HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\", serviceNames.size());
			}

			int result = MessageBoxA(dispatchContext.m_hWnd, confirmMsg.c_str(), "Confirm Registry Key Deletion", MB_YESNO | MB_ICONWARNING);
			if (result == IDYES) {
				spdlog::info("Starting async operation: Delete registry keys for {} service(s)", serviceNames.size());

				if (dispatchContext.m_pAsyncOp) {
					dispatchContext.m_pAsyncOp->Wait();
					delete dispatchContext.m_pAsyncOp;
					dispatchContext.m_pAsyncOp = nullptr;
				}

				dispatchContext.m_pAsyncOp = new AsyncOperation();
				dispatchContext.m_bShowProgressDialog = true;

				dispatchContext.m_pAsyncOp->Start(dispatchContext.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
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
					}
					catch (const std::exception& e) {
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
			if (!dispatchContext.m_selectedObjects.empty()) {
				// Cast away const - the dialog needs non-const pointers for editing
				std::vector<ServiceInfo*> services;
				for (const auto* svc : dispatchContext.m_selectedObjects) {
					services.push_back(const_cast<ServiceInfo*>(static_cast<const ServiceInfo*>(svc)));
				}
				m_pPropertiesDialog->Open(services);
			}
		}
		break;

		default:
			// Delegate to base class for common actions
			DispatchCommonAction(action, dispatchContext);
			break;
		}
	}
	*/
} // namespace pserv
