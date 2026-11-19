#include "precomp.h"
#include <core/async_operation.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <models/service_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/service_manager.h>

namespace pserv
{

    // Forward declare to avoid circular dependency
    class ServicesDataController;

    namespace
    {

        // Helper function to get ServiceInfo from DataObject
        inline const ServiceInfo *GetServiceInfo(const DataObject *obj)
        {
            return static_cast<const ServiceInfo *>(obj);
        }

        // ============================================================================
        // Service Lifecycle Actions
        // ============================================================================

        class ServiceStartAction final : public DataAction
        {
        public:
            ServiceStartAction()
                : DataAction{"Start", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return GetServiceInfo(obj)->GetCurrentState() == SERVICE_STOPPED;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Start {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            size_t successCount = 0;
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                float progressRange = 1.0f / static_cast<float>(total);

                                op->ReportProgress(baseProgress, std::format("Starting service '{}'... ({}/{})", serviceName, i + 1, total));

                                bool success = ServiceManager::StartServiceByName(serviceName,
                                    [op, baseProgress, progressRange](float progress, std::string message) -> bool
                                    {
                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                        return !op->IsCancelRequested();
                                    });
                                if (success)
                                    successCount++;
                            }

                            if (successCount == total)
                                op->ReportProgress(1.0f, std::format("Started {} service(s) successfully", total));
                            else if (successCount == 0)
                                op->ReportProgress(1.0f, std::format("Failed to start {} service(s)", total));
                            else
                                op->ReportProgress(1.0f, std::format("Started {} of {} service(s)", successCount, total));
                            return successCount > 0;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to start service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServiceStopAction final : public DataAction
        {
        public:
            ServiceStopAction()
                : DataAction{"Stop", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                DWORD state = GetServiceInfo(obj)->GetCurrentState();
                return (state == SERVICE_RUNNING || state == SERVICE_PAUSED);
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Stop {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            size_t successCount = 0;
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                float progressRange = 1.0f / static_cast<float>(total);

                                op->ReportProgress(baseProgress, std::format("Stopping service '{}'... ({}/{})", serviceName, i + 1, total));

                                bool success = ServiceManager::StopServiceByName(serviceName,
                                    [op, baseProgress, progressRange](float progress, std::string message) -> bool
                                    {
                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                        return !op->IsCancelRequested();
                                    });
                                if (success)
                                    successCount++;
                            }

                            if (successCount == total)
                                op->ReportProgress(1.0f, std::format("Stopped {} service(s) successfully", total));
                            else if (successCount == 0)
                                op->ReportProgress(1.0f, std::format("Failed to stop {} service(s)", total));
                            else
                                op->ReportProgress(1.0f, std::format("Stopped {} of {} service(s)", successCount, total));
                            return successCount > 0;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to stop service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServiceRestartAction final : public DataAction
        {
        public:
            ServiceRestartAction()
                : DataAction{"Restart", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return GetServiceInfo(obj)->GetCurrentState() == SERVICE_RUNNING;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Restart {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            size_t successCount = 0;
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                float progressRange = 1.0f / static_cast<float>(total);

                                op->ReportProgress(baseProgress, std::format("Restarting service '{}'... ({}/{})", serviceName, i + 1, total));

                                bool success = ServiceManager::RestartServiceByName(serviceName,
                                    [op, baseProgress, progressRange](float progress, std::string message) -> bool
                                    {
                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                        return !op->IsCancelRequested();
                                    });
                                if (success)
                                    successCount++;
                            }

                            if (successCount == total)
                                op->ReportProgress(1.0f, std::format("Restarted {} service(s) successfully", total));
                            else if (successCount == 0)
                                op->ReportProgress(1.0f, std::format("Failed to restart {} service(s)", total));
                            else
                                op->ReportProgress(1.0f, std::format("Restarted {} of {} service(s)", successCount, total));
                            return successCount > 0;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to restart service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServicePauseAction final : public DataAction
        {
        public:
            ServicePauseAction()
                : DataAction{"Pause", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *svc = GetServiceInfo(obj);
                return (svc->GetCurrentState() == SERVICE_RUNNING) && (svc->GetControlsAccepted() & SERVICE_ACCEPT_PAUSE_CONTINUE);
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Pause {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            size_t successCount = 0;
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                float progressRange = 1.0f / static_cast<float>(total);

                                op->ReportProgress(baseProgress, std::format("Pausing service '{}'... ({}/{})", serviceName, i + 1, total));

                                bool success = ServiceManager::PauseServiceByName(serviceName,
                                    [op, baseProgress, progressRange](float progress, std::string message) -> bool
                                    {
                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                        return !op->IsCancelRequested();
                                    });
                                if (success)
                                    successCount++;
                            }

                            if (successCount == total)
                                op->ReportProgress(1.0f, std::format("Paused {} service(s) successfully", total));
                            else if (successCount == 0)
                                op->ReportProgress(1.0f, std::format("Failed to pause {} service(s)", total));
                            else
                                op->ReportProgress(1.0f, std::format("Paused {} of {} service(s)", successCount, total));
                            return successCount > 0;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to pause service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServiceResumeAction final : public DataAction
        {
        public:
            ServiceResumeAction()
                : DataAction{"Resume", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return GetServiceInfo(obj)->GetCurrentState() == SERVICE_PAUSED;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Resume {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            size_t successCount = 0;
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float baseProgress = static_cast<float>(i) / static_cast<float>(total);
                                float progressRange = 1.0f / static_cast<float>(total);

                                op->ReportProgress(baseProgress, std::format("Resuming service '{}'... ({}/{})", serviceName, i + 1, total));

                                bool success = ServiceManager::ResumeServiceByName(serviceName,
                                    [op, baseProgress, progressRange](float progress, std::string message) -> bool
                                    {
                                        op->ReportProgress(baseProgress + progress * progressRange, message);
                                        return !op->IsCancelRequested();
                                    });
                                if (success)
                                    successCount++;
                            }

                            if (successCount == total)
                                op->ReportProgress(1.0f, std::format("Resumed {} service(s) successfully", total));
                            else if (successCount == 0)
                                op->ReportProgress(1.0f, std::format("Failed to resume {} service(s)", total));
                            else
                                op->ReportProgress(1.0f, std::format("Resumed {} of {} service(s)", successCount, total));
                            return successCount > 0;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to resume service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        // ============================================================================
        // Startup Type Configuration Actions
        // ============================================================================
        class ServiceSetStartupAction : public DataAction
        {
        public:
            ServiceSetStartupAction(std::string name, DWORD dwStartupAction)
                : DataAction{std::move(name), ActionVisibility::ContextMenu},
                  m_dwStartupAction{dwStartupAction}
            {
            }

        private:
            DWORD m_dwStartupAction;

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                spdlog::info("Starting async operation: Set startup type to {} for {} service(s)", m_dwStartupAction, serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                DWORD dwStartupAction{m_dwStartupAction};

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames, dwStartupAction](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float progress = static_cast<float>(i) / static_cast<float>(total);

                                op->ReportProgress(progress, std::format("Setting startup type for '{}'... ({}/{})", serviceName, i + 1, total));

                                ServiceManager::ChangeServiceStartType(serviceName, dwStartupAction);
                            }

                            op->ReportProgress(1.0f, std::format("Set startup type to Automatic for {} service(s)", total));
                            return true;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to change startup type: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServiceSetStartupAutomaticAction final : public ServiceSetStartupAction
        {
        public:
            ServiceSetStartupAutomaticAction()
                : ServiceSetStartupAction{"Set Startup: Automatic", SERVICE_AUTO_START}
            {
            }
        };

        class ServiceSetStartupManualAction final : public ServiceSetStartupAction
        {
        public:
            ServiceSetStartupManualAction()
                : ServiceSetStartupAction{"Set Startup: Manual", SERVICE_DEMAND_START}
            {
            }
        };

        class ServiceSetStartupDisabledAction final : public ServiceSetStartupAction
        {
        public:
            ServiceSetStartupDisabledAction()
                : ServiceSetStartupAction{"Set Startup: Disabled", SERVICE_DISABLED}
            {
            }
        };

        // ============================================================================
        // System Integration Actions
        // ============================================================================

        class ServiceOpenInRegistryEditorAction final : public DataAction
        {
        public:
            ServiceOpenInRegistryEditorAction()
                : DataAction{"Open in Registry Editor", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::string serviceName = GetServiceInfo(ctx.m_selectedObjects[0])->GetName();
                std::string regPath = std::format("SYSTEM\\CurrentControlSet\\Services\\{}", serviceName);

                HKEY hKey;
                LSTATUS status = RegCreateKeyExW(
                    HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
                if (status == ERROR_SUCCESS)
                {
                    std::wstring fullPath = utils::Utf8ToWide(std::format("HKEY_LOCAL_MACHINE\\{}", regPath));
                    status = RegSetValueExW(hKey, L"LastKey", 0, REG_SZ, (const BYTE *)fullPath.c_str(), (DWORD)(fullPath.length() + 1) * sizeof(wchar_t));
                    if (status != ERROR_SUCCESS)
                    {
                        LogWin32ErrorCode("RegSetValueExW", status, "setting LastKey for service '{}'", serviceName);
                    }
                    RegCloseKey(hKey);
                }
                else
                {
                    LogWin32ErrorCode("RegCreateKeyExW", status, "opening Regedit settings key");
                }

                spdlog::info("Opening registry editor for: {}", serviceName);
                HINSTANCE result = ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOW);
                if (reinterpret_cast<INT_PTR>(result) <= 32)
                {
                    LogWin32Error("ShellExecuteW", "opening regedit.exe");
                }
            }
        };

        class ServiceOpenInExplorerAction final : public DataAction
        {
        public:
            ServiceOpenInExplorerAction()
                : DataAction{"Open in Explorer", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::string installLocation = GetServiceInfo(ctx.m_selectedObjects[0])->GetInstallLocation();
                if (!installLocation.empty())
                {
                    spdlog::info("Opening explorer: {}", installLocation);
                    std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
                    HINSTANCE result = ShellExecuteW(NULL, L"open", wInstallLocation.c_str(), NULL, NULL, SW_SHOW);
                    if (reinterpret_cast<INT_PTR>(result) <= 32)
                    {
                        LogWin32Error("ShellExecuteW", "opening install location '{}'", installLocation);
                    }
                }
                else
                {
                    spdlog::warn("No install location available for this service");
                }
            }
        };

        class ServiceOpenTerminalHereAction final : public DataAction
        {
        public:
            ServiceOpenTerminalHereAction()
                : DataAction{"Open Terminal Here", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::string installLocation = GetServiceInfo(ctx.m_selectedObjects[0])->GetInstallLocation();
                if (!installLocation.empty())
                {
                    spdlog::info("Opening terminal in: {}", installLocation);
                    std::wstring wInstallLocation = utils::Utf8ToWide(installLocation);
                    HINSTANCE result = ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, wInstallLocation.c_str(), SW_SHOW);
                    if (reinterpret_cast<INT_PTR>(result) <= 32)
                    {
                        LogWin32Error("ShellExecuteW", "opening terminal in '{}'", installLocation);
                    }
                }
                else
                {
                    spdlog::warn("No install location available for this service");
                }
            }
        };

        // ============================================================================
        // Dangerous Actions
        // ============================================================================

        class ServiceUninstallAction final : public DataAction
        {
        public:
            ServiceUninstallAction()
                : DataAction{"Uninstall Service", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            bool IsDestructive() const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                // Show confirmation dialog
                std::string confirmMsg;
                if (serviceNames.size() == 1)
                {
                    confirmMsg =
                        std::format("Are you sure you want to delete the service '{}'?\n\nThis will remove the service from the system.", serviceNames[0]);
                }
                else
                {
                    confirmMsg = std::format(
                        "Are you sure you want to delete {} services?\n\nThis will remove all selected services from the system.", serviceNames.size());
                }

                int result = MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Service Deletion", MB_YESNO | MB_ICONWARNING);
                if (result != IDYES)
                    return;

                spdlog::info("Starting async operation: Delete {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float progress = static_cast<float>(i) / static_cast<float>(total);

                                op->ReportProgress(progress, std::format("Deleting service '{}'... ({}/{})", serviceName, i + 1, total));

                                ServiceManager::DeleteService(serviceName);
                            }

                            op->ReportProgress(1.0f, std::format("Deleted {} service(s) successfully", total));
                            return true;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to delete service: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        class ServiceDeleteRegistryKeyAction final : public DataAction
        {
        public:
            ServiceDeleteRegistryKeyAction()
                : DataAction{"Delete Registry Key", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            bool IsDestructive() const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<std::string> serviceNames;
                for (const auto *svc : ctx.m_selectedObjects)
                {
                    serviceNames.push_back(GetServiceInfo(svc)->GetName());
                }

                // Show confirmation dialog
                std::string confirmMsg;
                if (serviceNames.size() == 1)
                {
                    confirmMsg = std::format("Are you sure you want to delete the registry key for service '{}'?\n\n"
                                             "This will remove: HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\{}\n\n"
                                             "This is typically used to clean up orphaned service registry entries.",
                        serviceNames[0],
                        serviceNames[0]);
                }
                else
                {
                    confirmMsg = std::format("Are you sure you want to delete the registry keys for {} services?\n\n"
                                             "This will remove the registry entries under HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\",
                        serviceNames.size());
                }

                int result = MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Registry Key Deletion", MB_YESNO | MB_ICONWARNING);
                if (result != IDYES)
                    return;

                spdlog::info("Starting async operation: Delete registry keys for {} service(s)", serviceNames.size());

                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [serviceNames](AsyncOperation *op) -> bool
                    {
                        try
                        {
                            size_t total = serviceNames.size();
                            for (size_t i = 0; i < total; ++i)
                            {
                                const std::string &serviceName = serviceNames[i];
                                float progress = static_cast<float>(i) / static_cast<float>(total);

                                op->ReportProgress(progress, std::format("Deleting registry key for '{}'... ({}/{})", serviceName, i + 1, total));

                                std::wstring wServiceName = utils::Utf8ToWide(serviceName);
                                std::wstring keyPath = std::format(L"SYSTEM\\CurrentControlSet\\Services\\{}", wServiceName);

                                LSTATUS result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str());
                                if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
                                {
                                    LogWin32ErrorCode("RegDeleteTreeW", result, "deleting registry key for service '{}'", serviceName);
                                    return false;
                                }
                            }

                            op->ReportProgress(1.0f, std::format("Deleted registry keys for {} service(s) successfully", total));
                            return true;
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("Failed to delete registry key: {}", e.what());
                            return false;
                        }
                    });
            }
        };

        ServiceStartAction theServiceStartAction;
        ServiceStopAction theServiceStopAction;
        ServiceRestartAction theServiceRestartAction;
        ServicePauseAction theServicePauseAction;
        ServiceResumeAction theServiceResumeAction;
        ServiceSetStartupAutomaticAction theServiceSetStartupAutomaticAction;
        ServiceSetStartupManualAction theServiceSetStartupManualAction;
        ServiceSetStartupDisabledAction theServiceSetStartupDisabledAction;
        ServiceOpenInRegistryEditorAction theServiceOpenInRegistryEditorAction;
        ServiceOpenInExplorerAction theServiceOpenInExplorerAction;
        ServiceOpenTerminalHereAction theServiceOpenTerminalHereAction;
        ServiceUninstallAction theServiceUninstallAction;
        ServiceDeleteRegistryKeyAction theServiceDeleteRegistryKeyAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Functions
    // ============================================================================

    std::vector<const DataAction *> CreateServiceActions(DWORD currentState, DWORD controlsAccepted)
    {
        std::vector<const DataAction *> actions{};

        // State-dependent actions first
        if (currentState == SERVICE_STOPPED)
        {
            actions.push_back(&theServiceStartAction);
        }
        else if (currentState == SERVICE_RUNNING)
        {
            actions.push_back(&theServiceStopAction);
            actions.push_back(&theServiceRestartAction);

            // Only offer Pause if service accepts pause/continue
            if (controlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE)
            {
                actions.push_back(&theServicePauseAction);
            }
        }
        else if (currentState == SERVICE_PAUSED)
        {
            actions.push_back(&theServiceResumeAction);
            actions.push_back(&theServiceStopAction);
        }

        // Separator before startup type actions
        actions.push_back(&theDataActionSeparator);

        // Startup type actions (always available).
        // TODO: add only those that are NOT currently set (i.e. if is automatic, no need to add "set to automatic")
        actions.push_back(&theServiceSetStartupAutomaticAction);
        actions.push_back(&theServiceSetStartupManualAction);
        actions.push_back(&theServiceSetStartupDisabledAction);

        // Separator before file system integration actions
        actions.push_back(&theDataActionSeparator);

        // File system integration actions (always available)
        actions.push_back(&theServiceOpenInRegistryEditorAction);
        actions.push_back(&theServiceOpenInExplorerAction);
        actions.push_back(&theServiceOpenTerminalHereAction);

        // Separator before deletion actions
        actions.push_back(&theDataActionSeparator);

        // Deletion actions (always available)
        actions.push_back(&theServiceUninstallAction);
        actions.push_back(&theServiceDeleteRegistryKeyAction);
        return actions;
    }

#ifdef PSERV_CONSOLE_BUILD
    // Console: Return ALL possible actions regardless of state (for command registration)
    std::vector<const DataAction *> CreateAllServiceActions()
    {
        return {
            &theServiceStartAction,
            &theServiceStopAction,
            &theServiceRestartAction,
            &theServicePauseAction,
            &theServiceResumeAction,
            &theDataActionSeparator,
            &theServiceSetStartupAutomaticAction,
            &theServiceSetStartupManualAction,
            &theServiceSetStartupDisabledAction,
            &theDataActionSeparator,
            &theServiceOpenInRegistryEditorAction,
            &theServiceOpenInExplorerAction,
            &theServiceOpenTerminalHereAction,
            &theDataActionSeparator,
            &theServiceUninstallAction,
            &theServiceDeleteRegistryKeyAction};
    }
#endif

} // namespace pserv
