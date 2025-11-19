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

namespace pserv {

UninstallerDataController::UninstallerDataController()
    : DataController("Uninstaller", "Program")
    , m_pPropertiesDialog(new UninstallerPropertiesDialog())
{
    m_columns = {
        DataObjectColumn("Display Name", "DisplayName"),
        DataObjectColumn("Version", "Version"),
        DataObjectColumn("Publisher", "Publisher"),
        DataObjectColumn("Install Location", "InstallLocation"),
        DataObjectColumn("Uninstall String", "UninstallString"),
        DataObjectColumn("Install Date", "InstallDate"),
        DataObjectColumn("Estimated Size", "EstimatedSize"),
        DataObjectColumn("Comments", "Comments"),
        DataObjectColumn("Help Link", "HelpLink"),
        DataObjectColumn("URL Info About", "URLInfoAbout")
    };
}

UninstallerDataController::~UninstallerDataController() {
    Clear();
    delete m_pPropertiesDialog;
}

void UninstallerDataController::Refresh() {
    spdlog::info("Refreshing installed programs...");
    Clear();

    m_programs = UninstallerManager::EnumerateInstalledPrograms();
    
    // Update base class pointers
    m_dataObjects.reserve(m_programs.size());
    for (auto* program : m_programs) {
        m_dataObjects.push_back(program);
    }

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
    m_dataObjects.clear();
    m_bLoaded = false;
}

const std::vector<DataObjectColumn>& UninstallerDataController::GetColumns() const {
    return m_columns;
}

const std::vector<DataObject*>& UninstallerDataController::GetDataObjects() const {
    return m_dataObjects;
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

    return actions;
}

std::string UninstallerDataController::GetActionName(int action) const {
    switch (static_cast<UninstallerAction>(action)) {
        case UninstallerAction::Properties: return "Properties...";
        case UninstallerAction::Uninstall: return "Uninstall";
        default: return "";
    }
}

void UninstallerDataController::DispatchAction(int action, DataActionDispatchContext& context) {
    if (context.m_selectedObjects.empty()) return;

    auto uninstallerAction = static_cast<UninstallerAction>(action);
    const InstalledProgramInfo* program = static_cast<const InstalledProgramInfo*>(context.m_selectedObjects[0]);

    switch (uninstallerAction) {
        case UninstallerAction::Properties: {
            // Cast away constness for the dialog, as it expects non-const pointers
            std::vector<InstalledProgramInfo*> programsToOpen;
            for (const auto* obj : context.m_selectedObjects) {
                programsToOpen.push_back(const_cast<InstalledProgramInfo*>(static_cast<const InstalledProgramInfo*>(obj)));
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
                
                // Need to parse the uninstall string. It can be just a path, or start with msiexec.exe, rundll32.exe, etc.
                std::string uninstallCmd = program->GetUninstallString();
                std::string command = uninstallCmd;
                std::string args;

                size_t firstSpace = uninstallCmd.find(' ');
                if (firstSpace != std::string::npos) {
                    command = uninstallCmd.substr(0, firstSpace);
                    args = uninstallCmd.substr(firstSpace + 1);
                }

                // Remove quotes from command if present
                if (command.length() >= 2 && command.front() == '"' && command.back() == '"') {
                    command = command.substr(1, command.length() - 2);
                }

                // Convert to wide strings for ShellExecuteW
                std::wstring wCommand = pserv::utils::Utf8ToWide(command);
                std::wstring wArgs = pserv::utils::Utf8ToWide(args);

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
    }
}

void UninstallerDataController::RenderPropertiesDialog() {
    if (m_pPropertiesDialog) {
        m_pPropertiesDialog->Render();
    }
}

void UninstallerDataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) return;

    m_lastSortColumn = columnIndex;
    m_lastSortAscending = ascending;

    std::sort(m_programs.begin(), m_programs.end(), 
        [columnIndex, ascending](const InstalledProgramInfo* a, const InstalledProgramInfo* b) {
        int comparison = 0;
        switch (columnIndex) {
            case 0: comparison = a->GetDisplayName().compare(b->GetDisplayName()); break;
            case 1: comparison = a->GetDisplayVersion().compare(b->GetDisplayVersion()); break;
            case 2: comparison = a->GetPublisher().compare(b->GetPublisher()); break;
            case 3: comparison = a->GetInstallLocation().compare(b->GetInstallLocation()); break;
            case 4: comparison = a->GetUninstallString().compare(b->GetUninstallString()); break;
            case 5: comparison = a->GetInstallDate().compare(b->GetInstallDate()); break;
            case 6: // Estimated Size - need numeric comparison if possible
                // For now, string compare. Future: parse to actual numbers.
                comparison = a->GetEstimatedSize().compare(b->GetEstimatedSize());
                break;
            case 7: comparison = a->GetComments().compare(b->GetComments()); break;
            case 8: comparison = a->GetHelpLink().compare(b->GetHelpLink()); break;
            case 9: comparison = a->GetUrlInfoAbout().compare(b->GetUrlInfoAbout()); break;
            default: comparison = 0; break;
        }

        return ascending ? (comparison < 0) : (comparison > 0);
    });
}

} // namespace pserv