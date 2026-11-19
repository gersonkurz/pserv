/// @file processes_data_controller.h
/// @brief Controller for running processes enumeration.
///
/// Provides viewing and management of running processes using the
/// Windows Tool Help and process APIs.
#pragma once
#include <core/data_controller.h>

namespace pserv
{
    /// @brief Data controller for running processes.
    ///
    /// Enumerates all running processes on the system, displaying:
    /// - Process name, ID, and parent process
    /// - Memory usage and thread count
    /// - Executable path and owning user
    ///
    /// Processes owned by the current user are highlighted for easy identification.
    class ProcessesDataController : public DataController
    {
    public:
        ProcessesDataController();

    private:
        // Refresh() is the base class's; this controller only enumerates:
        void Collect(DataObjectContainer &target, const RefreshRequest &request) override;

        /// The current user's name is UI-side state (it drives GetVisualState), so it is updated
        /// on the UI thread after each publish rather than by the refresh worker.
        void OnPublished() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

        std::vector<const DataAction *> GetAllActions() const override;

        VisualState GetVisualState(const DataObject *dataObject) const override;

    private:
        std::string m_currentUserName; ///< Cached username for highlighting own processes.
    };

} // namespace pserv
