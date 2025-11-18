#include "precomp.h"
#include "windows_data_controller.h"
#include "../windows_api/window_manager.h"
#include "../utils/string_utils.h"
#include <algorithm>

namespace pserv {

WindowsDataController::WindowsDataController()
    : DataController("Windows", "Window")
{
    m_columns = {
        DataObjectColumn("InternalID", "InternalID"),
        DataObjectColumn("Title", "Title"),
        DataObjectColumn("Class", "Class"),
        DataObjectColumn("Size", "Size"),
        DataObjectColumn("Position", "Position"),
        DataObjectColumn("Style", "Style"),
        DataObjectColumn("ExStyle", "ExStyle"),
        DataObjectColumn("ID", "ID"),
        DataObjectColumn("ProcessID", "ProcessID"),
        DataObjectColumn("ThreadID", "ThreadID"),
        DataObjectColumn("Process", "Process")
    };

    // Initial refresh
    Refresh();
}

WindowsDataController::~WindowsDataController() {
    Clear();
}

void WindowsDataController::Refresh() {
    spdlog::info("Refreshing windows...");
    
    // Get new list
    std::vector<WindowInfo*> newWindows = WindowManager::EnumerateWindows();
    
    // Using basic replace for now
    Clear();
    m_windows = std::move(newWindows);
    
    // Update base class pointers
    m_dataObjects.reserve(m_windows.size());
    for (auto* win : m_windows) {
        m_dataObjects.push_back(win);
    }
    
    spdlog::info("Refreshed {} windows", m_windows.size());
}

void WindowsDataController::Clear() {
    for (auto* win : m_windows) {
        delete win;
    }
    m_windows.clear();
    m_dataObjects.clear();
}

const std::vector<DataObjectColumn>& WindowsDataController::GetColumns() const {
    return m_columns;
}

const std::vector<DataObject*>& WindowsDataController::GetDataObjects() const {
    return m_dataObjects;
}

std::vector<int> WindowsDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(WindowAction::Show));
    actions.push_back(static_cast<int>(WindowAction::Hide));
    actions.push_back(static_cast<int>(WindowAction::Minimize));
    actions.push_back(static_cast<int>(WindowAction::Maximize));
    actions.push_back(static_cast<int>(WindowAction::Restore));
    
    actions.push_back(static_cast<int>(WindowAction::Separator));
    
    actions.push_back(static_cast<int>(WindowAction::BringToFront));
    actions.push_back(static_cast<int>(WindowAction::Close));

    return actions;
}

std::string WindowsDataController::GetActionName(int action) const {
    switch (static_cast<WindowAction>(action)) {
        case WindowAction::Show: return "Show";
        case WindowAction::Hide: return "Hide";
        case WindowAction::Minimize: return "Minimize";
        case WindowAction::Maximize: return "Maximize";
        case WindowAction::Restore: return "Restore";
        case WindowAction::BringToFront: return "Bring To Front";
        case WindowAction::Close: return "Close";
        default: return "";
    }
}

VisualState WindowsDataController::GetVisualState(const DataObject* dataObject) const {
    if (!dataObject) return VisualState::Normal;
    
    // Gray out if disabled (invisible or 0-size) - takes precedence
    if (dataObject->IsDisabled()) {
        return VisualState::Disabled;
    }
    
    // Highlight if running (normal, responsive window)
    if (dataObject->IsRunning()) {
        return VisualState::Highlighted;
    }
    
    return VisualState::Normal;
}

void WindowsDataController::DispatchAction(int action, DataActionDispatchContext& context) {
    if (context.m_selectedObjects.empty()) return;

    auto winAction = static_cast<WindowAction>(action);
    int successCount = 0;

    for (const auto* obj : context.m_selectedObjects) {
        const auto* win = static_cast<const WindowInfo*>(obj);
        HWND hwnd = win->GetHandle();
        bool result = false;

        switch (winAction) {
            case WindowAction::Show:
                result = WindowManager::ShowWindow(hwnd, SW_SHOW);
                break;
            case WindowAction::Hide:
                result = WindowManager::ShowWindow(hwnd, SW_HIDE);
                break;
            case WindowAction::Minimize:
                result = WindowManager::ShowWindow(hwnd, SW_MINIMIZE);
                break;
            case WindowAction::Maximize:
                result = WindowManager::ShowWindow(hwnd, SW_MAXIMIZE);
                break;
            case WindowAction::Restore:
                result = WindowManager::ShowWindow(hwnd, SW_RESTORE);
                break;
            case WindowAction::BringToFront:
                result = WindowManager::BringToFront(hwnd);
                break;
            case WindowAction::Close:
                result = WindowManager::CloseWindow(hwnd);
                break;
        }
        
        if (result) successCount++;
    }
    
    if (successCount > 0) {
        // Auto-refresh to show new state
        Refresh();
    }
}

void WindowsDataController::RenderPropertiesDialog() {
    // No properties dialog for windows yet
}

void WindowsDataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) return;

    std::sort(m_dataObjects.begin(), m_dataObjects.end(), [columnIndex, ascending](const DataObject* a, const DataObject* b) {
        std::string valA = a->GetProperty(columnIndex);
        std::string valB = b->GetProperty(columnIndex);
        
        // TODO: Numeric sort for IDs?
        
        int cmp = valA.compare(valB);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

} // namespace pserv
