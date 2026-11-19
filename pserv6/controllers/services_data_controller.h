/// @file services_data_controller.h
/// @brief Controller for Windows services management.
///
/// Provides viewing, starting, stopping, and configuring Windows services
/// on local or remote machines via the Service Control Manager API.
#pragma once
#include <core/data_controller.h>

namespace pserv
{
    /// @brief Data controller for Windows services.
    ///
    /// This controller enumerates and manages Windows services using the
    /// Service Control Manager (SCM) API. It supports:
    /// - Viewing service status, configuration, and dependencies
    /// - Starting, stopping, pausing, and resuming services
    /// - Changing service startup type and other properties
    /// - Remote machine connection
    ///
    /// @note Also serves as base class for DevicesDataController.
    class ServicesDataController : public DataController
    {
    private:
        const DWORD m_serviceType{SERVICE_WIN32 | SERVICE_DRIVER}; ///< Filter: SERVICE_WIN32, SERVICE_DRIVER, or both.
        std::string m_machineName; ///< Target machine (empty = local).

        /// @brief Buffer for staged property edits before commit.
        struct EditBuffer
        {
            std::string displayName;
            std::string description;
            std::string startType;
            std::string binaryPathName;
            DWORD startTypeValue{SERVICE_NO_CHANGE};
        };
        EditBuffer m_editBuffer;
        DataObject *m_editingObject{nullptr}; ///< Object being edited, if any.

    public:
        /// @brief Construct a services controller.
        /// @param serviceType SERVICE_WIN32, SERVICE_DRIVER, or combined flags.
        /// @param viewName Display name for the view (defaults to "Services").
        /// @param itemName Singular item name (defaults to "service").
        ServicesDataController(DWORD serviceType = SERVICE_WIN32, const char *viewName = nullptr, const char *itemName = nullptr);

        /// @brief Set target machine for remote service management.
        /// @param machineName Machine name (empty string for local machine).
        void SetMachineName(const std::string& machineName);

        /// @brief Get the current target machine name.
        const std::string& GetMachineName() const { return m_machineName; }

    private:
        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *service) const override;

        void BeginPropertyEdits(DataObject *obj) override;
        bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue) override;
        bool CommitPropertyEdits(DataObject *obj) override;
        std::vector<std::string> GetComboOptions(int columnIndex) const override;
    };

} // namespace pserv
