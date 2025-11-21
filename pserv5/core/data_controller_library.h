#pragma once
#include <core/data_object.h>
#include <core/data_object_column.h>
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
