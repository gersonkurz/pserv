#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_action_dispatch_context.h>
#include <models/installed_program_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>

namespace pserv
{

    // Forward declare to avoid circular dependency
    class UninstallerDataController;

    namespace
    {

        // Helper function to get InstalledProgramInfo from DataObject
        inline const InstalledProgramInfo *GetProgramInfo(const DataObject *obj)
        {
            return static_cast<const InstalledProgramInfo *>(obj);
        }

        // ============================================================================
        // Uninstall Action
        // ============================================================================

        class UninstallProgramAction final : public DataAction
        {
        public:
            UninstallProgramAction()
                : DataAction{"Uninstall", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return !GetProgramInfo(obj)->GetUninstallString().empty();
            }

            bool IsDestructive() const override
            {
                return true;
            }

            bool RequiresConfirmation() const override
            {
                return true;
            }

            std::string GetConfirmationText(const std::vector<DataObject *> &selection) const override
            {
                const auto *program = selection.empty() ? nullptr : GetProgramInfo(selection[0]);
                return std::format("Are you sure you want to uninstall '{}'?\n\nThis will launch the program's uninstaller.",
                    program ? program->GetDisplayName() : std::string{});
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *program = GetProgramInfo(ctx.m_selectedObjects[0]);

                if (program->GetUninstallString().empty())
                {
                    throw std::runtime_error(std::format("Cannot uninstall '{}': its uninstall string is empty.", program->GetDisplayName()));
                }

                spdlog::info("Launching uninstaller for '{}': {}", program->GetDisplayName(), program->GetUninstallString());

                // Parse the uninstall string using Windows API for proper quote handling
                std::string uninstallCmd = program->GetUninstallString();
                std::wstring wUninstallCmd = pserv::utils::Utf8ToWide(uninstallCmd);

                int argc = 0;
                wil::unique_hlocal_ptr<LPWSTR[]> argv(CommandLineToArgvW(wUninstallCmd.c_str(), &argc));

                if (!argv || argc == 0)
                {
                    throw std::runtime_error(std::format("Failed to parse uninstall command: {}", uninstallCmd));
                }

                    // First argument is the command, rest are arguments
                    std::wstring wCommand = argv.get()[0];
                    std::wstring wArgs;

                    // Rebuild arguments from remaining tokens
                    for (int i = 1; i < argc; ++i)
                    {
                        if (i > 1)
                            wArgs += L" ";

                        // Quote arguments that contain spaces
                        std::wstring arg = argv.get()[i];
                        if (arg.find(L' ') != std::wstring::npos)
                        {
                            wArgs += L"\"" + arg + L"\"";
                        }
                        else
                        {
                            wArgs += arg;
                        }
                    }

                // ShellExecuteW will handle elevation if needed and is generally robust
                HINSTANCE result = ShellExecuteW(ctx.m_hWnd, L"open", wCommand.c_str(), wArgs.empty() ? nullptr : wArgs.c_str(), nullptr, SW_SHOWNORMAL);
                if (reinterpret_cast<intptr_t>(result) <= 32)
                {
                    std::string errorMsg = pserv::utils::GetLastWin32ErrorMessage();
                    throw std::runtime_error(std::format("Failed to launch uninstaller: {}", errorMsg));
                }
                else
                {
                    // Uninstaller launched successfully. Signal that refresh is needed.
                    spdlog::info("Uninstaller launched for '{}'. User should refresh after uninstaller completes.", program->GetDisplayName());
                    // Note: m_bNeedsRefresh will be set by the controller after we return
                }
            }
        };

        UninstallProgramAction theUninstallProgramAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateUninstallerActions()
    {
        return {&theUninstallProgramAction};
    }

    std::vector<const DataAction *> CreateAllUninstallerActions()
    {
        // Console: Return all actions regardless of program state
        return {&theUninstallProgramAction};
    }

} // namespace pserv
