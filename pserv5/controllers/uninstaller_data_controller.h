/// @file uninstaller_data_controller.h
/// @brief Controller for installed programs enumeration.
///
/// Lists installed programs from the Windows registry uninstall keys
/// and provides uninstall/modify actions.
#pragma once
#include <core/data_controller.h>

namespace pserv
{
    /// @brief Data controller for installed programs.
    ///
    /// Reads the Windows registry uninstall keys to enumerate installed
    /// software, displaying:
    /// - Program name, publisher, and version
    /// - Install date and size
    /// - Uninstall command string
    ///
    /// Provides actions to run the program's uninstaller or modify installation.
    class UninstallerDataController : public DataController
    {
    public:
        UninstallerDataController();

    private:
        void Refresh(bool isAutoRefresh = false) override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif
    };

} // namespace pserv
