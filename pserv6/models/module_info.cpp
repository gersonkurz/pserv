#include "precomp.h"
#include <models/module_info.h>
#include <utils/format_utils.h>
#include <utils/string_utils.h>

namespace pserv
{

    ModuleInfo::ModuleInfo(uint32_t processId, const std::string &name)
        : m_processId{processId},
          m_baseAddress{nullptr},
          m_name{name}
    {
        SetRunning(true);
        SetDisabled(false);
    }

    void ModuleInfo::SetValues(void *baseAddress, uint32_t size, const std::string &path)
    {
        m_baseAddress = baseAddress;
        m_size = size;
        m_path = path;
    }
    
    PropertyValue ModuleInfo::GetTypedProperty(int propertyId) const
    {
        switch (static_cast<ModuleProperty>(propertyId))
        {
        case ModuleProperty::BaseAddress:
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m_baseAddress));
        case ModuleProperty::Size:
            return static_cast<uint64_t>(m_size);
        case ModuleProperty::Name:
        case ModuleProperty::Path:
            return GetProperty(propertyId);
        case ModuleProperty::ProcessId:
            return static_cast<uint64_t>(m_processId);
        default:
            return std::monostate{};
        }
    }

    std::string ModuleInfo::GetProperty(int column) const
    {
        switch (static_cast<ModuleProperty>(column))
        {
        case ModuleProperty::BaseAddress:
            return std::format("{:#x}", reinterpret_cast<uintptr_t>(m_baseAddress));
        case ModuleProperty::Size:
            return utils::FormatSize(m_size);
        case ModuleProperty::Name:
            return m_name;
        case ModuleProperty::Path:
            return m_path;
        case ModuleProperty::ProcessId:
            return std::to_string(m_processId);
        default:
            return "";
        }
    }

    bool ModuleInfo::MatchesFilter(const std::string &filter) const
    {
        // filter is pre-lowercased by caller
        return pserv::utils::ToLower(m_name).find(filter) != std::string::npos || pserv::utils::ToLower(m_path).find(filter) != std::string::npos ||
               std::to_string(m_processId).find(filter) != std::string::npos;
    }

} // namespace pserv
