#include "precomp.h"
#include <actions/environment_variable_actions.h>
#include <controllers/environment_variables_data_controller.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <models/environment_variable_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/environment_variable_manager.h>

namespace pserv
{

    namespace
    {

        // Helper function to get EnvironmentVariableInfo from DataObject
        inline const EnvironmentVariableInfo *GetEnvVarInfo(const DataObject *obj)
        {
            return static_cast<const EnvironmentVariableInfo *>(obj);
        }

        // ============================================================================
        // Copy Actions
        // ============================================================================

        class EnvVarCopyValueAction final : public DataAction
        {
        public:
            EnvVarCopyValueAction()
                : DataAction{"Copy Value", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Copy Value' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *envVar = GetEnvVarInfo(ctx.m_selectedObjects[0]);
                utils::CopyToClipboard(envVar->GetValue().c_str());
                spdlog::info("Copied environment variable value to clipboard: {}", envVar->GetName());
#endif
            }
        };

        class EnvVarCopyNameAction final : public DataAction
        {
        public:
            EnvVarCopyNameAction()
                : DataAction{"Copy Name", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Copy Name' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *envVar = GetEnvVarInfo(ctx.m_selectedObjects[0]);
                utils::CopyToClipboard(envVar->GetName().c_str());
                spdlog::info("Copied environment variable name to clipboard: {}", envVar->GetName());
#endif
            }
        };

        // ============================================================================
        // Delete Action
        // ============================================================================

        class EnvVarDeleteAction final : public DataAction
        {
        public:
            EnvVarDeleteAction()
                : DataAction{"Delete", ActionVisibility::Both}
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

            bool RequiresConfirmation() const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<const EnvironmentVariableInfo *> envVars;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    envVars.push_back(GetEnvVarInfo(obj));
                }

#ifndef PSERV_CONSOLE_BUILD
                // GUI: Show confirmation dialog
                std::string confirmMsg = "Are you sure you want to delete the following environment variables?\n\n";
                for (const auto *envVar : envVars)
                {
                    confirmMsg += std::format("{} ({})\n", envVar->GetName(), envVar->GetScopeString());
                    if (envVars.size() >= 10)
                    {
                        confirmMsg += "... and more\n";
                        break;
                    }
                }
                if (MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Deletion", MB_YESNO | MB_ICONWARNING) != IDYES)
                {
                    return;
                }
#endif
                // Console: --force flag already checked by pservc.cpp

                int success = 0;
                for (const auto *envVar : envVars)
                {
                    if (EnvironmentVariableManager::DeleteEnvironmentVariable(envVar->GetName(), envVar->GetScope()))
                    {
                        success++;
                    }
                }
                spdlog::info("Deleted {}/{} environment variables", success, envVars.size());
                ctx.m_bNeedsRefresh = true;
            }
        };

        // ============================================================================
        // Add New Variable Action
        // ============================================================================

        class EnvVarAddAction final : public DataAction
        {
        private:
            EnvironmentVariableScope m_scope;

        public:
            EnvVarAddAction(std::string name, EnvironmentVariableScope scope)
                : DataAction{std::move(name), ActionVisibility::ContextMenu},
                  m_scope{scope}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifndef PSERV_CONSOLE_BUILD
                // Show the add variable dialog
                auto *controller = static_cast<EnvironmentVariablesDataController *>(ctx.m_pController);
                controller->ShowAddVariableDialog(m_scope);
#else
                spdlog::error("Adding new environment variables is not supported in console mode");
                throw std::runtime_error("Adding new environment variables is not supported in console mode. Use the GUI application.");
#endif
            }
        };

        // ============================================================================
        // Open in Registry Editor Action
        // ============================================================================

        class EnvVarOpenInRegistryAction final : public DataAction
        {
        public:
            EnvVarOpenInRegistryAction()
                : DataAction{"Open in Registry Editor", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Open in Registry Editor' is not supported in console build");
                throw std::runtime_error("This action requires GUI and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *envVar = GetEnvVarInfo(ctx.m_selectedObjects[0]);

                std::string regPath;
                if (envVar->GetScope() == EnvironmentVariableScope::System)
                {
                    regPath = "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
                }
                else
                {
                    regPath = "Environment";
                }

                HKEY hKey;
                LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
                    0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);

                if (status == ERROR_SUCCESS)
                {
                    std::wstring fullPath;
                    if (envVar->GetScope() == EnvironmentVariableScope::System)
                    {
                        fullPath = utils::Utf8ToWide(std::format("HKEY_LOCAL_MACHINE\\{}", regPath));
                    }
                    else
                    {
                        fullPath = utils::Utf8ToWide(std::format("HKEY_CURRENT_USER\\{}", regPath));
                    }

                    status = RegSetValueExW(hKey, L"LastKey", 0, REG_SZ,
                        reinterpret_cast<const BYTE *>(fullPath.c_str()),
                        static_cast<DWORD>((fullPath.length() + 1) * sizeof(wchar_t)));

                    if (status != ERROR_SUCCESS)
                    {
                        LogWin32ErrorCode("RegSetValueExW", status, "setting LastKey for environment variable");
                    }
                    RegCloseKey(hKey);
                }
                else
                {
                    LogWin32ErrorCode("RegCreateKeyExW", status, "opening Regedit settings key");
                }

                spdlog::info("Opening registry editor for environment variable: {}", envVar->GetName());
                HINSTANCE result = ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOW);
                if (reinterpret_cast<INT_PTR>(result) <= 32)
                {
                    LogWin32Error("ShellExecuteW", "opening regedit.exe");
                }
#endif
            }
        };

        // Static action instances
        EnvVarCopyValueAction theCopyValueAction;
        EnvVarCopyNameAction theCopyNameAction;
        EnvVarDeleteAction theDeleteAction;
        EnvVarOpenInRegistryAction theOpenInRegistryAction;
        EnvVarAddAction theAddSystemVariableAction{"Add System Variable", EnvironmentVariableScope::System};
        EnvVarAddAction theAddUserVariableAction{"Add User Variable", EnvironmentVariableScope::User};

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateEnvironmentVariableActions(EnvironmentVariableScope scope)
    {
        std::vector<const DataAction *> actions;

        // Copy actions
        actions.push_back(&theCopyValueAction);
        actions.push_back(&theCopyNameAction);

        actions.push_back(&theDataActionSeparator);

        // Add actions (scope-specific)
        if (scope == EnvironmentVariableScope::System)
        {
            actions.push_back(&theAddSystemVariableAction);
        }
        else
        {
            actions.push_back(&theAddUserVariableAction);
        }

        actions.push_back(&theDataActionSeparator);

        // Registry action
        actions.push_back(&theOpenInRegistryAction);

        actions.push_back(&theDataActionSeparator);

        // Destructive actions
        actions.push_back(&theDeleteAction);

        return actions;
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllEnvironmentVariableActions()
    {
        // Console: Return all actions regardless of variable scope
        return {
            &theCopyValueAction,
            &theCopyNameAction,
            &theDataActionSeparator,
            &theAddSystemVariableAction,
            &theAddUserVariableAction,
            &theDataActionSeparator,
            &theOpenInRegistryAction,
            &theDataActionSeparator,
            &theDeleteAction};
    }
#endif

} // namespace pserv
