#include "precomp.h"
#include <actions/window_actions.h>
#include <controllers/windows_data_controller.h>
#include <utils/string_utils.h>
#include <windows_api/window_manager.h>
#include <models/window_info.h>

namespace pserv
{

    WindowsDataController::WindowsDataController()
        : DataController{WINDOWS_DATA_CONTROLLER_NAME,
              "Window",
              {{"HWND", "HWND", ColumnDataType::UnsignedInteger},
                  {"Title", "Title", ColumnDataType::String},
                  {"Class", "Class", ColumnDataType::String},
                  {"Size", "Size", ColumnDataType::String},
                  {"Position", "Position", ColumnDataType::String},
                  {"Style", "Style", ColumnDataType::UnsignedInteger},
                  {"ExStyle", "ExStyle", ColumnDataType::UnsignedInteger},
                  {"ID", "ID", ColumnDataType::UnsignedInteger},
                  {"ProcessID", "ProcessID", ColumnDataType::UnsignedInteger},
                  {"ThreadID", "ThreadID", ColumnDataType::UnsignedInteger},
                  {"Process", "Process", ColumnDataType::String}}}
    {
    }

    void WindowsDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing windows...");

        m_objects.StartRefresh();
        WindowManager::EnumerateWindows(&m_objects);
        m_objects.FinishRefresh();

        spdlog::info("Refreshed {} windows", m_objects.GetSize());
        m_bLoaded = true;
    }

    std::vector<const DataAction *> WindowsDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateWindowActions();
    }

    VisualState WindowsDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
            return VisualState::Normal;

        // Gray out if disabled (invisible or 0-size) - takes precedence
        if (dataObject->IsDisabled())
        {
            return VisualState::Disabled;
        }

        // Highlight if running (normal, responsive window)
        if (dataObject->IsRunning())
        {
            return VisualState::Highlighted;
        }

        return VisualState::Normal;
    }

} // namespace pserv
