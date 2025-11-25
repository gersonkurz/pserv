/// @file devices_data_controller.h
/// @brief Controller for Windows device drivers.
///
/// Specialized view of services filtered to show only kernel and
/// file system drivers (SERVICE_DRIVER type).
#pragma once
#include <controllers/services_data_controller.h>

namespace pserv
{
    /// @brief Data controller for Windows device drivers.
    ///
    /// This is a specialized ServicesDataController that filters for
    /// driver services only (kernel drivers and file system drivers).
    /// Inherits all service management functionality from the base class.
    class DevicesDataController final : public ServicesDataController
    {
    public:
        DevicesDataController()
            : ServicesDataController{SERVICE_DRIVER, DEVICES_DATA_CONTROLLER_NAME.data(), "Device"}
        {
        }
    };

} // namespace pserv
