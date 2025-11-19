#include "precomp.h"
#include "uninstaller_data_controller.h"
#include "../windows_api/uninstaller_manager.h"
#include "../utils/string_utils.h"
#include "../utils/win32_error.h"
#include <spdlog/spdlog.h>
#include <format>
#include <algorithm>
#include <imgui.h>
#include <shellapi.h>
#include <wil/resource.h>

namespace pserv {

UninstallerDataController::UninstallerDataController()
    : DataController{"Uninstaller", "Program", {
        {"Display Name", "DisplayName", ColumnDataType::String},
        {"Version", "Version", ColumnDataType::String},
        {"Publisher", "Publisher", ColumnDataType::String},
        {"Install Location", "InstallLocation", ColumnDataType::String},
        {"Uninstall String", "UninstallString", ColumnDataType::String},
        {"Install Date", "InstallDate", ColumnDataType::String},
        {"Estimated Size", "EstimatedSize", ColumnDataType::Size},
        {"Comments", "Comments", ColumnDataType::String},
        {"Help Link", "HelpLink", ColumnDataType::String},
        {"URL Info About", "URLInfoAbout", ColumnDataType::String}
    } }
    , m_pPropertiesDialog{new UninstallerPropertiesDialog()}
{
}

UninstallerDataController::~UninstallerDataController() {
    Clear();
    delete m_pPropertiesDialog;
}

void UninstallerDataController::Refresh() {
    spdlog::info("Refreshing installed programs...");
    Clear();

    m_programs = UninstallerManager::EnumerateInstalledPrograms();

    // Re-apply last sort order if any
    if (m_lastSortColumn >= 0) {
        Sort(m_lastSortColumn, m_lastSortAscending);
    }

    spdlog::info("Refreshed {} installed programs", m_programs.size());
    m_bLoaded = true;
}

void UninstallerDataController::Clear() {
    for (auto* program : m_programs) {
        delete program;
    }
    m_programs.clear();
    m_bLoaded = false;
}
const std::vector<DataObject*>& UninstallerDataController::GetDataObjects() const {
    return reinterpret_cast<const std::vector<DataObject*>&>(m_programs);
}

VisualState UninstallerDataController::GetVisualState(const DataObject* dataObject) const {
    // No special visual states for installed programs currently
    return VisualState::Normal;
}

std::vector<int> UninstallerDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(UninstallerAction::Properties));
    actions.push_back(static_cast<int>(UninstallerAction::Separator));
    actions.push_back(static_cast<int>(UninstallerAction::Uninstall));

    // Add common export/copy actions
    AddCommonExportActions(actions);

    return actions;
}

std::string UninstallerDataController::GetActionName(int action) const {
    switch (static_cast<UninstallerAction>(action)) {
        case UninstallerAction::Properties: return "Properties...";
        case UninstallerAction::Uninstall: return "Uninstall";
        default:
            std::string commonName = GetCommonActionName(action);
            return !commonName.empty() ? commonName : "";
    }
}

void UninstallerDataController::DispatchAction(int action, DataActionDispatchContext& context) {
    if (context.m_selectedObjects.empty()) return;

    auto uninstallerAction = static_cast<UninstallerAction>(action);
    const InstalledProgramInfo* program = static_cast<const InstalledProgramInfo*>(context.m_selectedObjects[0]);

    switch (uninstallerAction) {
        case UninstallerAction::Properties: {
            // Dialog accepts const pointers (read-only)
            std::vector<const InstalledProgramInfo*> programsToOpen;
            for (const auto* obj : context.m_selectedObjects) {
                programsToOpen.push_back(static_cast<const InstalledProgramInfo*>(obj));
            }
            m_pPropertiesDialog->Open(programsToOpen);
            break;
        }
        case UninstallerAction::Uninstall: {
            if (program->GetUninstallString().empty()) {
                spdlog::warn("Cannot uninstall '{}': UninstallString is empty.", program->GetDisplayName());
                MessageBoxA(context.m_hWnd, "Uninstall string is empty. Cannot proceed with uninstallation.", "Uninstallation Error", MB_OK | MB_ICONERROR);
                break;
            }

            std::string confirmMsg = std::format("Are you sure you want to uninstall '{}'?\n\nThis will launch the program's uninstaller.", program->GetDisplayName());
            if (MessageBoxA(context.m_hWnd, confirmMsg.c_str(), "Confirm Uninstallation", MB_YESNO | MB_ICONWARNING) == IDYES) {
                spdlog::info("Launching uninstaller for '{}': {}", program->GetDisplayName(), program->GetUninstallString());

                // Parse the uninstall string using Windows API for proper quote handling
                std::string uninstallCmd = program->GetUninstallString();
                std::wstring wUninstallCmd = pserv::utils::Utf8ToWide(uninstallCmd);

                int argc = 0;
                wil::unique_hlocal_ptr<LPWSTR[]> argv(CommandLineToArgvW(wUninstallCmd.c_str(), &argc));

                if (!argv || argc == 0) {
                    spdlog::error("Failed to parse uninstall command: {}", uninstallCmd);
                    MessageBoxA(context.m_hWnd, "Failed to parse uninstall command.", "Uninstallation Error", MB_OK | MB_ICONERROR);
                    break;
                }

                // First argument is the command, rest are arguments
                std::wstring wCommand = argv.get()[0];
                std::wstring wArgs;

                // Rebuild arguments from remaining tokens
                for (int i = 1; i < argc; ++i) {
                    if (i > 1) wArgs += L" ";

                    // Quote arguments that contain spaces
                    std::wstring arg = argv.get()[i];
                    if (arg.find(L' ') != std::wstring::npos) {
                        wArgs += L"\"" + arg + L"\"";
                    } else {
                        wArgs += arg;
                    }
                }

                // ShellExecuteW will handle elevation if needed and is generally robust
                HINSTANCE result = ShellExecuteW(context.m_hWnd, L"open", wCommand.c_str(), wArgs.empty() ? nullptr : wArgs.c_str(), nullptr, SW_SHOWNORMAL);
                if (reinterpret_cast<intptr_t>(result) <= 32) {
                    std::string errorMsg = pserv::utils::GetLastWin32ErrorMessage();
                    spdlog::error("Failed to launch uninstaller for '{}': {}", program->GetDisplayName(), errorMsg);
                    MessageBoxA(context.m_hWnd, std::format("Failed to launch uninstaller. Error: {}.", errorMsg).c_str(), "Uninstallation Error", MB_OK | MB_ICONERROR);
                } else {
                    // Uninstaller launched successfully. Signal that refresh is needed.
                    spdlog::info("Uninstaller launched for '{}'. User should refresh after uninstaller completes.", program->GetDisplayName());
                    m_bNeedsRefresh = true;
                }
            }
            break;
        }

        default:
            // Delegate to base class for common actions
            DispatchCommonAction(action, context);
            break;
    }
}

void UninstallerDataController::RenderPropertiesDialog() {
    if (m_pPropertiesDialog) {
        m_pPropertiesDialog->Render();
    }
}

} // namespace pserv