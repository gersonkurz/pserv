/// @file windows_data_controller.h
/// @brief Controller for top-level window enumeration.
///
/// Lists all visible top-level windows with their properties and
/// provides window manipulation actions.
#pragma once
#include <core/data_controller.h>

namespace pserv
{
    /// @brief Data controller for desktop windows.
    ///
    /// Enumerates all top-level windows on the desktop, showing:
    /// - Window title and class name
    /// - Owning process information
    /// - Window state (visible, minimized, etc.)
    ///
    /// @note Auto-refresh is disabled because the window list changes
    ///       too rapidly, which would be distracting during normal use.
    class WindowsDataController : public DataController
    {
    public:
        WindowsDataController();

    private:
        void Refresh(bool isAutoRefresh = false) override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        bool SupportsAutoRefresh() const override { return false; }
    };

} // namespace pserv
