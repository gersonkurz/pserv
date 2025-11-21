#include "precomp.h"
#include <controllers/windows_data_controller.h>
#include <windows_api/window_manager.h>
#include <utils/string_utils.h>
#include <actions/window_actions.h>

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
{
}

void WindowsDataController::Refresh() {
    spdlog::info("Refreshing windows...");
    
    m_objects = WindowManager::EnumerateWindows();

    spdlog::info("Refreshed {} windows", m_objects.size());
    m_bLoaded = true;
}

std::vector<const DataAction*> WindowsDataController::GetActions(const DataObject* dataObject) const {
    return CreateWindowActions();
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

/*
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
*/

} // namespace pserv
