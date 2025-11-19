#include "precomp.h"
#include "installed_program_info.h"
#include "../utils/string_utils.h"
#include <format>

namespace pserv {

InstalledProgramInfo::InstalledProgramInfo(
    std::string displayName,
    std::string displayVersion,
    std::string publisher,
    std::string installLocation,
    std::string uninstallString,
    std::string installDate,
    std::string estimatedSize,
    std::string comments,
    std::string helpLink,
    std::string urlInfoAbout,
    uint64_t estimatedSizeBytes)
    : m_displayName(std::move(displayName))
    , m_displayVersion(std::move(displayVersion))
    , m_publisher(std::move(publisher))
    , m_installLocation(std::move(installLocation))
    , m_uninstallString(std::move(uninstallString))
    , m_installDate(std::move(installDate))
    , m_estimatedSize(std::move(estimatedSize))
    , m_estimatedSizeBytes(estimatedSizeBytes)
    , m_comments(std::move(comments))
    , m_helpLink(std::move(helpLink))
    , m_urlInfoAbout(std::move(urlInfoAbout))
{
    // Set flags for DataObject base class
    SetRunning(false);  // Programs aren't 'running' in the same sense as services/processes
    SetDisabled(false); // Programs aren't 'disabled'
}

std::string InstalledProgramInfo::GetId() const {
    // A combination of DisplayName and UninstallString should be unique enough
    return std::format("{}_{}", m_displayName, m_uninstallString);
}

void InstalledProgramInfo::Update(const DataObject& other) {
    // Dynamic updates for installed programs are rare/not implemented yet
    // But we must override the pure virtual function
}

PropertyValue InstalledProgramInfo::GetTypedProperty(int propertyId) const {
    switch (propertyId) {
        case 6: // Estimated Size
            return m_estimatedSizeBytes;
        case 0: // Display Name
        case 1: // Version
        case 2: // Publisher
        case 3: // Install Location
        case 4: // Uninstall String
        case 5: // Install Date
        case 7: // Comments
        case 8: // Help Link
        case 9: // URL Info About
            return GetProperty(propertyId);
        default:
            return std::monostate{};
    }
}

std::string InstalledProgramInfo::GetProperty(int columnIndex) const {
    switch (columnIndex) {
        case 0: return m_displayName;
        case 1: return m_displayVersion;
        case 2: return m_publisher;
        case 3: return m_installLocation;
        case 4: return m_uninstallString;
        case 5: return m_installDate;
        case 6: return m_estimatedSize;
        case 7: return m_comments;
        case 8: return m_helpLink;
        case 9: return m_urlInfoAbout;
        default: return "";
    }
}

bool InstalledProgramInfo::MatchesFilter(const std::string& filter) const {
    if (filter.empty()) return true;
    
    // Simple case-insensitive search in Display Name and Publisher
    std::string lowerFilter = pserv::utils::ToLower(filter);
    
    if (pserv::utils::ToLower(m_displayName).find(lowerFilter) != std::string::npos) return true;
    if (pserv::utils::ToLower(m_publisher).find(lowerFilter) != std::string::npos) return true;
    
    return false;
}

} // namespace pserv