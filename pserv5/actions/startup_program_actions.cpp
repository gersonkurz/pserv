#include "precomp.h"
#include <actions/startup_program_actions.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <models/startup_program_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/startup_program_manager.h>

namespace pserv
{

    namespace
    {

        // Helper function to get StartupProgramInfo from DataObject
        inline StartupProgramInfo *GetStartupProgramInfo(DataObject *obj)
        {
            return static_cast<StartupProgramInfo *>(obj);
        }

        inline const StartupProgramInfo *GetStartupProgramInfo(const DataObject *obj)
        {
            return static_cast<const StartupProgramInfo *>(obj);
        }

        // ============================================================================
        // Enable/Disable Actions
        // ============================================================================

        class StartupProgramEnableAction final : public DataAction
        {
        public:
            StartupProgramEnableAction()
                : DataAction{"Enable", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *program = GetStartupProgramInfo(obj);
                return !program->IsEnabled() && program->GetType() != StartupProgramType::StartupFolder;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int success = 0;
                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *program = GetStartupProgramInfo(obj);
                    if (StartupProgramManager::SetEnabled(program, true))
                    {
                        success++;
                    }
                }
                spdlog::info("Enabled {}/{} startup programs", success, ctx.m_selectedObjects.size());
            }
        };

        class StartupProgramDisableAction final : public DataAction
        {
        public:
            StartupProgramDisableAction()
                : DataAction{"Disable", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *program = GetStartupProgramInfo(obj);
                return program->IsEnabled() && program->GetType() != StartupProgramType::StartupFolder;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int success = 0;
                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *program = GetStartupProgramInfo(obj);
                    if (StartupProgramManager::SetEnabled(program, false))
                    {
                        success++;
                    }
                }
                spdlog::info("Disabled {}/{} startup programs", success, ctx.m_selectedObjects.size());
            }
        };

        // ============================================================================
        // Delete Action
        // ============================================================================

        class StartupProgramDeleteAction final : public DataAction
        {
        public:
            StartupProgramDeleteAction()
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
                std::vector<StartupProgramInfo *> programs;

                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *program = GetStartupProgramInfo(obj);
                    programs.push_back(program);
                }

#ifndef PSERV_CONSOLE_BUILD
                // GUI: Show confirmation dialog
                std::string confirmMsg = "Are you sure you want to delete the following startup programs?\n\n";
                for (auto *program : programs)
                {
                    confirmMsg += std::format("{} ({})\n", program->GetName(), program->GetLocation());
                    if (programs.size() >= 10)
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
                for (auto *program : programs)
                {
                    if (StartupProgramManager::DeleteStartupProgram(program))
                    {
                        success++;
                    }
                }
                spdlog::info("Deleted {}/{} startup programs", success, programs.size());

                // TODO: Trigger refresh
            }
        };

        // ============================================================================
        // Copy Actions
        // ============================================================================

        class StartupProgramCopyCommandAction final : public DataAction
        {
        public:
            StartupProgramCopyCommandAction()
                : DataAction{"Copy Command", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                // Console: Not supported (requires clipboard)
                spdlog::error("'Copy Command' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *program = GetStartupProgramInfo(ctx.m_selectedObjects[0]);
                utils::CopyToClipboard(program->GetCommand().c_str());
                spdlog::info("Copied startup program command to clipboard: {}", program->GetName());
#endif
            }
        };

        class StartupProgramCopyNameAction final : public DataAction
        {
        public:
            StartupProgramCopyNameAction()
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
                // Console: Not supported (requires clipboard)
                spdlog::error("'Copy Name' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *program = GetStartupProgramInfo(ctx.m_selectedObjects[0]);
                utils::CopyToClipboard(program->GetName().c_str());
                spdlog::info("Copied startup program name to clipboard: {}", program->GetName());
#endif
            }
        };

        // ============================================================================
        // Open Location Actions
        // ============================================================================

        class StartupProgramOpenLocationAction final : public DataAction
        {
        public:
            StartupProgramOpenLocationAction()
                : DataAction{"Open File Location", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *program = GetStartupProgramInfo(obj);
                return program->GetType() == StartupProgramType::StartupFolder && !program->GetFilePath().empty();
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                // Console: Not supported (requires GUI)
                spdlog::error("'Open File Location' is not supported in console build");
                throw std::runtime_error("This action requires GUI and is not available in console mode");
#else
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    const auto *program = GetStartupProgramInfo(obj);
                    if (!program->GetFilePath().empty())
                    {
                        std::wstring wPath = utils::Utf8ToWide(program->GetFilePath());
                        std::wstring cmd = L"/select,\"" + wPath + L"\"";
                        HINSTANCE result = ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL, SW_SHOW);
                        if (reinterpret_cast<INT_PTR>(result) <= 32)
                        {
                            LogWin32Error("ShellExecuteW", "opening file location for '{}'", program->GetFilePath());
                        }
                    }
                }
#endif
            }
        };

        class StartupProgramOpenInRegistryAction final : public DataAction
        {
        public:
            StartupProgramOpenInRegistryAction()
                : DataAction{"Open in Registry Editor", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *program = GetStartupProgramInfo(obj);
                return program->GetType() != StartupProgramType::StartupFolder;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                // Console: Not supported (requires GUI)
                spdlog::error("'Open in Registry Editor' is not supported in console build");
                throw std::runtime_error("This action requires GUI and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *program = GetStartupProgramInfo(ctx.m_selectedObjects[0]);

                HKEY hKey;
                LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
                    0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);

                if (status == ERROR_SUCCESS)
                {
                    std::wstring fullPath;
                    if (program->GetScope() == StartupProgramScope::System)
                    {
                        fullPath = utils::Utf8ToWide(std::format("HKEY_LOCAL_MACHINE\\{}", program->GetRegistryPath()));
                    }
                    else
                    {
                        fullPath = utils::Utf8ToWide(std::format("HKEY_CURRENT_USER\\{}", program->GetRegistryPath()));
                    }

                    status = RegSetValueExW(hKey, L"LastKey", 0, REG_SZ,
                        reinterpret_cast<const BYTE *>(fullPath.c_str()),
                        static_cast<DWORD>((fullPath.length() + 1) * sizeof(wchar_t)));

                    if (status != ERROR_SUCCESS)
                    {
                        LogWin32ErrorCode("RegSetValueExW", status, "setting LastKey for startup program");
                    }
                    RegCloseKey(hKey);
                }
                else
                {
                    LogWin32ErrorCode("RegCreateKeyExW", status, "opening Regedit settings key");
                }

                spdlog::info("Opening registry editor for startup program: {}", program->GetName());
                HINSTANCE result = ShellExecuteW(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOW);
                if (reinterpret_cast<INT_PTR>(result) <= 32)
                {
                    LogWin32Error("ShellExecuteW", "opening regedit.exe");
                }
#endif
            }
        };

        // Static action instances
        StartupProgramEnableAction theEnableAction;
        StartupProgramDisableAction theDisableAction;
        StartupProgramDeleteAction theDeleteAction;
        StartupProgramCopyCommandAction theCopyCommandAction;
        StartupProgramCopyNameAction theCopyNameAction;
        StartupProgramOpenLocationAction theOpenLocationAction;
        StartupProgramOpenInRegistryAction theOpenInRegistryAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateStartupProgramActions(StartupProgramType type, bool enabled)
    {
        std::vector<const DataAction *> actions;

        // Enable/Disable actions (only for registry items)
        if (type != StartupProgramType::StartupFolder)
        {
            if (enabled)
            {
                actions.push_back(&theDisableAction);
            }
            else
            {
                actions.push_back(&theEnableAction);
            }

            actions.push_back(&theDataActionSeparator);
        }

        // Copy actions
        actions.push_back(&theCopyCommandAction);
        actions.push_back(&theCopyNameAction);

        actions.push_back(&theDataActionSeparator);

        // Open location actions
        if (type == StartupProgramType::StartupFolder)
        {
            actions.push_back(&theOpenLocationAction);
        }
        else
        {
            actions.push_back(&theOpenInRegistryAction);
        }

        actions.push_back(&theDataActionSeparator);

        // Delete action
        actions.push_back(&theDeleteAction);

        return actions;
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllStartupProgramActions()
    {
        // Console: Return all actions regardless of program type/state
        return {
            &theEnableAction,
            &theDisableAction,
            &theDataActionSeparator,
            &theCopyCommandAction,
            &theCopyNameAction,
            &theDataActionSeparator,
            &theOpenLocationAction,
            &theOpenInRegistryAction,
            &theDataActionSeparator,
            &theDeleteAction};
    }
#endif

} // namespace pserv
