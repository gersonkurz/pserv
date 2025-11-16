#pragma once
#include "services_data_controller.h"

namespace pserv {

// DevicesDataController is just ServicesDataController filtered for driver services
class DevicesDataController : public ServicesDataController {
public:
    DevicesDataController()
        : ServicesDataController(
            SERVICE_KERNEL_DRIVER | SERVICE_FILE_SYSTEM_DRIVER,
            "Devices",
            "Device")
    {
    }
};

} // namespace pserv
