#include "precomp.h"
#include <core/data_controller_library.h>
#include <controllers/services_data_controller.h>
#include <controllers/devices_data_controller.h>

namespace
{
	std::vector<pserv::DataController*> dataControllers;
}

namespace pserv
{
	DataControllerLibrary::~DataControllerLibrary()
	{
		for(const auto ptr: dataControllers)
		{
			delete ptr;
		}
		dataControllers.clear();
	}

	const std::vector<DataController*>& DataControllerLibrary::GetDataControllers()
	{
		// layz initialization
		if (dataControllers.empty())
		{
			dataControllers.push_back(new ServicesDataController());
			dataControllers.push_back(new DevicesDataController());
		}
		return dataControllers;
	}
}