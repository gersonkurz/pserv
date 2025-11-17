#pragma once
#include "data_object.h"
#include "data_object_column.h"
#include <vector>
#include <memory>
#include <core/data_controller.h>

namespace pserv {
	class DataControllerLibrary final {

	public:
		DataControllerLibrary() = default;
		~DataControllerLibrary();

		// Core abstract methods
		const std::vector<DataController*>& GetDataControllers();
	};
}
