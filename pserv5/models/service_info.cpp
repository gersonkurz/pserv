#include "precomp.h"
#include <models/service_info.h>
#include <utils/string_utils.h>

namespace pserv {

	ServiceInfo::ServiceInfo(std::string name, std::string displayName, DWORD currentState, DWORD serviceType)
		: m_name{ std::move(name) }
		, m_displayName{ std::move(displayName) }
		, m_currentState{ currentState }
		, m_startType{}
		, m_processId{}
		, m_serviceType(serviceType)
		, m_controlsAccepted{}
		, m_errorControl{}
		, m_tagId{}
		, m_win32ExitCode{}
		, m_serviceSpecificExitCode{}
		, m_checkPoint{}
		, m_waitHint{}
		, m_serviceFlags{}
	{
		// Update running state based on service state
		SetRunning(m_currentState == SERVICE_RUNNING);
	}

	bool ServiceInfo::MatchesFilter(const std::string& filter) const
	{
		if (filter.empty()) return true;

		// filter is pre-lowercased by caller
		if (utils::ToLower(m_name).find(filter) != std::string::npos) return true;
		if (utils::ToLower(m_displayName).find(filter) != std::string::npos) return true;
		if (utils::ToLower(m_description).find(filter) != std::string::npos) return true;
		if (utils::ToLower(m_binaryPathName).find(filter) != std::string::npos) return true;
		if (utils::ToLower(m_user).find(filter) != std::string::npos) return true;

		return false;
	}

	void ServiceInfo::SetCurrentState(DWORD state) {
		m_currentState = state;
		SetRunning(m_currentState == SERVICE_RUNNING);
	}

	PropertyValue ServiceInfo::GetTypedProperty(int propertyId) const {
		switch (static_cast<ServiceProperty>(propertyId)) {
		case ServiceProperty::ProcessId:
			return static_cast<uint64_t>(m_processId);
		case ServiceProperty::TagId:
			return static_cast<uint64_t>(m_tagId);
		case ServiceProperty::Win32ExitCode:
			return static_cast<uint64_t>(m_win32ExitCode);
		case ServiceProperty::ServiceSpecificExitCode:
			return static_cast<uint64_t>(m_serviceSpecificExitCode);
		case ServiceProperty::CheckPoint:
			return static_cast<uint64_t>(m_checkPoint);
		case ServiceProperty::WaitHint:
			return static_cast<uint64_t>(m_waitHint);
		case ServiceProperty::ServiceFlags:
			return static_cast<uint64_t>(m_serviceFlags);
		case ServiceProperty::Name:
		case ServiceProperty::DisplayName:
		case ServiceProperty::Status:
		case ServiceProperty::StartType:
		case ServiceProperty::ServiceType:
		case ServiceProperty::BinaryPathName:
		case ServiceProperty::Description:
		case ServiceProperty::User:
		case ServiceProperty::LoadOrderGroup:
		case ServiceProperty::ErrorControl:
		case ServiceProperty::ControlsAccepted:
			return GetProperty(propertyId);
		default:
			return std::monostate{};
		}
	}

	std::string ServiceInfo::GetProperty(int propertyId) const {
		switch (static_cast<ServiceProperty>(propertyId)) {
		case ServiceProperty::Name:
			return m_name;
		case ServiceProperty::DisplayName:
			return m_displayName;
		case ServiceProperty::Status:
			return GetStatusString();
		case ServiceProperty::StartType:
			return GetStartTypeString();
		case ServiceProperty::ProcessId:
			return m_processId > 0 ? std::to_string(m_processId) : "";
		case ServiceProperty::ServiceType:
			return GetServiceTypeString();
		case ServiceProperty::BinaryPathName:
			return m_binaryPathName;
		case ServiceProperty::Description:
			return m_description;
		case ServiceProperty::User:
			return m_user;
		case ServiceProperty::LoadOrderGroup:
			return m_loadOrderGroup;
		case ServiceProperty::ErrorControl:
			return GetErrorControlString();
		case ServiceProperty::TagId:
			return m_tagId > 0 ? std::to_string(m_tagId) : "";
		case ServiceProperty::Win32ExitCode:
			return std::to_string(m_win32ExitCode);
		case ServiceProperty::ServiceSpecificExitCode:
			return std::to_string(m_serviceSpecificExitCode);
		case ServiceProperty::CheckPoint:
			return std::to_string(m_checkPoint);
		case ServiceProperty::WaitHint:
			return std::to_string(m_waitHint);
		case ServiceProperty::ServiceFlags:
			return std::to_string(m_serviceFlags);
		case ServiceProperty::ControlsAccepted:
			return GetControlsAcceptedString();
		default:
			return "";
		}
	}

	std::string ServiceInfo::GetStatusString() const {
		switch (m_currentState) {
		case SERVICE_STOPPED: return "Stopped";
		case SERVICE_START_PENDING: return "Start Pending";
		case SERVICE_STOP_PENDING: return "Stop Pending";
		case SERVICE_RUNNING: return "Running";
		case SERVICE_CONTINUE_PENDING: return "Continue Pending";
		case SERVICE_PAUSE_PENDING: return "Pause Pending";
		case SERVICE_PAUSED: return "Paused";
		default: return std::format("Unknown ({})", m_currentState);
		}
	}

	std::string ServiceInfo::GetStartTypeString() const {
		switch (m_startType) {
		case SERVICE_AUTO_START: return "Automatic";
		case SERVICE_BOOT_START: return "Boot";
		case SERVICE_DEMAND_START: return "Manual";
		case SERVICE_DISABLED: return "Disabled";
		case SERVICE_SYSTEM_START: return "System";
		default: return std::format("Unknown ({})", m_startType);
		}
	}

	std::string ServiceInfo::GetServiceTypeString() const {
		std::string result;

		if (m_serviceType & SERVICE_KERNEL_DRIVER) {
			result += "Kernel Driver";
		}
		if (m_serviceType & SERVICE_FILE_SYSTEM_DRIVER) {
			if (!result.empty()) result += " | ";
			result += "File System Driver";
		}
		if (m_serviceType & SERVICE_WIN32_OWN_PROCESS) {
			if (!result.empty()) result += " | ";
			result += "Win32 Own Process";
		}
		if (m_serviceType & SERVICE_WIN32_SHARE_PROCESS) {
			if (!result.empty()) result += " | ";
			result += "Win32 Share Process";
		}
		if (m_serviceType & SERVICE_INTERACTIVE_PROCESS) {
			if (!result.empty()) result += " | ";
			result += "Interactive";
		}

		return result.empty() ? std::format("Unknown (0x{:X})", m_serviceType) : result;
	}

	std::string ServiceInfo::GetErrorControlString() const {
		switch (m_errorControl) {
		case SERVICE_ERROR_IGNORE: return "Ignore";
		case SERVICE_ERROR_NORMAL: return "Normal";
		case SERVICE_ERROR_SEVERE: return "Severe";
		case SERVICE_ERROR_CRITICAL: return "Critical";
		default: return std::format("Unknown ({})", m_errorControl);
		}
	}

	std::string ServiceInfo::GetControlsAcceptedString() const {
		if (m_controlsAccepted == 0) {
			return "None";
		}

		std::string result;

		if (m_controlsAccepted & SERVICE_ACCEPT_STOP) {
			result += "Stop";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) {
			if (!result.empty()) result += " | ";
			result += "Pause/Continue";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_SHUTDOWN) {
			if (!result.empty()) result += " | ";
			result += "Shutdown";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_PARAMCHANGE) {
			if (!result.empty()) result += " | ";
			result += "Param Change";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_NETBINDCHANGE) {
			if (!result.empty()) result += " | ";
			result += "Net Bind Change";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_HARDWAREPROFILECHANGE) {
			if (!result.empty()) result += " | ";
			result += "Hardware Profile Change";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_POWEREVENT) {
			if (!result.empty()) result += " | ";
			result += "Power Event";
		}
		if (m_controlsAccepted & SERVICE_ACCEPT_SESSIONCHANGE) {
			if (!result.empty()) result += " | ";
			result += "Session Change";
		}

		return result;
	}

	std::string ServiceInfo::GetInstallLocation() const {
		if (m_binaryPathName.empty()) {
			return "";
		}

		std::string path = m_binaryPathName;

		// Remove quotes if present
		if (path.front() == '"') {
			size_t endQuote = path.find('"', 1);
			if (endQuote != std::string::npos) {
				path = path.substr(1, endQuote - 1);
			}
		}
		else {
			// No quotes - path might have arguments, take everything before first space
			size_t spacePos = path.find(' ');
			if (spacePos != std::string::npos) {
				path = path.substr(0, spacePos);
			}
		}

		// Extract directory
		size_t lastSlash = path.find_last_of("\\/");
		if (lastSlash != std::string::npos) {
			return path.substr(0, lastSlash);
		}

		return path;
	}

} // namespace pserv
