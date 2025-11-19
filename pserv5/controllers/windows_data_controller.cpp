#include "precomp.h"
#include <controllers/windows_data_controller.h>
#include <windows_api/window_manager.h>
#include <utils/string_utils.h>
#include <algorithm>

namespace pserv {

WindowsDataController::WindowsDataController()
    : DataController{"Windows", "Window", {
        {"InternalID", "InternalID", ColumnDataType::UnsignedInteger},
        {"Title", "Title", ColumnDataType::String},
        {"Class", "Class", ColumnDataType::String},
        {"Size", "Size", ColumnDataType::String},
        {"Position", "Position", ColumnDataType::String},
        {"Style", "Style", ColumnDataType::UnsignedInteger},
        {"ExStyle", "ExStyle", ColumnDataType::UnsignedInteger},
        {"ID", "ID", ColumnDataType::UnsignedInteger},
        {"ProcessID", "ProcessID", ColumnDataType::UnsignedInteger},
        {"ThreadID", "ThreadID", ColumnDataType::UnsignedInteger},
        {"Process", "Process", ColumnDataType::String}
    } }
    , m_pPropertiesDialog{new WindowPropertiesDialog()}
{
}

WindowsDataController::~WindowsDataController() {
    Clear();
    delete m_pPropertiesDialog;
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
    m_bLoaded = true;
}

void WindowsDataController::Clear() {
    for (auto* win : m_windows) {
        delete win;
    }
    m_windows.clear();
    m_dataObjects.clear();
    m_bLoaded = false;
}
const std::vector<DataObject*>& WindowsDataController::GetDataObjects() const {
    return m_dataObjects;
}

std::vector<int> WindowsDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(WindowAction::Properties));
    actions.push_back(static_cast<int>(WindowAction::Separator));

    actions.push_back(static_cast<int>(WindowAction::Show));
    actions.push_back(static_cast<int>(WindowAction::Hide));
    actions.push_back(static_cast<int>(WindowAction::Minimize));
    actions.push_back(static_cast<int>(WindowAction::Maximize));
    actions.push_back(static_cast<int>(WindowAction::Restore));
    
    actions.push_back(static_cast<int>(WindowAction::Separator));
    
    actions.push_back(static_cast<int>(WindowAction::BringToFront));
    actions.push_back(static_cast<int>(WindowAction::Close));

    // Add common export/copy actions
    AddCommonExportActions(actions);

    return actions;
}

std::string WindowsDataController::GetActionName(int action) const {
    switch (static_cast<WindowAction>(action)) {
        case WindowAction::Properties: return "Properties";
        case WindowAction::Show: return "Show";
        case WindowAction::Hide: return "Hide";
        case WindowAction::Minimize: return "Minimize";
        case WindowAction::Maximize: return "Maximize";
        case WindowAction::Restore: return "Restore";
        case WindowAction::BringToFront: return "Bring To Front";
        case WindowAction::Close: return "Close";
        default:
            std::string commonName = GetCommonActionName(action);
            return !commonName.empty() ? commonName : "";
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

    if (winAction == WindowAction::Properties) {
        std::vector<WindowInfo*> selected;
        for (const auto* obj : context.m_selectedObjects) {
            selected.push_back(const_cast<WindowInfo*>(static_cast<const WindowInfo*>(obj)));
        }
        m_pPropertiesDialog->Open(selected);
        return;
    }

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
            default:
                // Delegate to base class for common actions
                DispatchCommonAction(action, context);
                return;
        }

        if (result) successCount++;
    }

    if (successCount > 0) {
        // Auto-refresh to show new state
        Refresh();
    }
}

void WindowsDataController::RenderPropertiesDialog() {
    if (m_pPropertiesDialog && m_pPropertiesDialog->IsOpen()) {
        m_pPropertiesDialog->Render();
    }
}

} // namespace pserv
