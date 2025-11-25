/// @file startup_programs_data_controller.h
/// @brief Controller for Windows startup programs.
///
/// Enumerates programs configured to run at Windows startup from
/// various registry Run keys and Startup folders.
#pragma once
#include <core/data_controller.h>
#include <models/startup_program_info.h>

namespace pserv
{
    /// @brief Data controller for startup programs.
    ///
    /// Collects startup entries from multiple locations:
    /// - HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run
    /// - HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run
    /// - User and common Startup folders
    ///
    /// Displays program name, command line, and startup location.
    /// Disabled entries are shown grayed out.
    class StartupProgramsDataController : public DataController
    {
    public:
        StartupProgramsDataController();

    private:
        ~StartupProgramsDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
