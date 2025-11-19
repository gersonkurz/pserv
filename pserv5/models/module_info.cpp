#include "precomp.h"
#include "module_info.h"
#include "../utils/string_utils.h"
#include "../utils/format_utils.h"
#include <format>

namespace pserv {

ModuleInfo::ModuleInfo(
    uint32_t processId,
    void* baseAddress,
    uint32_t size,
    const std::string& name,
    const std::string& path
)
    : m_processId(processId)
    , m_baseAddress(baseAddress)
    , m_size(size)
    , m_name(name)
    , m_path(path)
{
    SetRunning(true);
    SetDisabled(false);
}

std::string ModuleInfo::GetId() const {
    return std::format("{}_{:#x}", m_processId, reinterpret_cast<uintptr_t>(m_baseAddress));
}

void ModuleInfo::Update(const DataObject& other) {
    // ModuleInfo properties are generally static once loaded, so no update logic for now.
    // If mutable properties were introduced, they would be updated here.
}

std::string ModuleInfo::GetProperty(int column) const {
    switch (column) {
        case 0: return std::format("{:#x}", reinterpret_cast<uintptr_t>(m_baseAddress));
        case 1: return utils::FormatSize(m_size);
        case 2: return m_name;
        case 3: return m_path;
        case 4: return std::to_string(m_processId);
        default: return "";
    }
}

bool ModuleInfo::MatchesFilter(const std::string& filter) const {
    std::string lowerFilter = pserv::utils::ToLower(filter);
    return pserv::utils::ToLower(m_name).find(lowerFilter) != std::string::npos ||
           pserv::utils::ToLower(m_path).find(lowerFilter) != std::string::npos ||
           std::to_string(m_processId).find(lowerFilter) != std::string::npos;
}

} // namespace pserv
