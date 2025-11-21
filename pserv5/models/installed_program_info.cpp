#include "precomp.h"
#include <models/installed_program_info.h>
#include <utils/string_utils.h>

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
    : m_displayName{std::move(displayName)}
    , m_displayVersion{std::move(displayVersion)}
    , m_publisher{std::move(publisher)}
    , m_installLocation{std::move(installLocation)}
    , m_uninstallString{std::move(uninstallString)}
    , m_installDate{std::move(installDate)}
    , m_estimatedSize{std::move(estimatedSize)}
    , m_estimatedSizeBytes{estimatedSizeBytes}
    , m_comments{std::move(comments)}
    , m_helpLink{std::move(helpLink)}
    , m_urlInfoAbout{std::move(urlInfoAbout)}
{
    // Set flags for DataObject base class
    SetRunning(false);  // Programs aren't 'running' in the same sense as services/processes
    SetDisabled(false); // Programs aren't 'disabled'
}

PropertyValue InstalledProgramInfo::GetTypedProperty(int propertyId) const {
    switch (static_cast<ProgramProperty>(propertyId)) {
        case ProgramProperty::EstimatedSize:
            return m_estimatedSizeBytes;
        case ProgramProperty::DisplayName:
        case ProgramProperty::Version:
        case ProgramProperty::Publisher:
        case ProgramProperty::InstallLocation:
        case ProgramProperty::UninstallString:
        case ProgramProperty::InstallDate:
        case ProgramProperty::Comments:
        case ProgramProperty::HelpLink:
        case ProgramProperty::URLInfoAbout:
            return GetProperty(propertyId);
        default:
            return std::monostate{};
    }
}

std::string InstalledProgramInfo::GetProperty(int columnIndex) const {
    switch (static_cast<ProgramProperty>(columnIndex)) {
        case ProgramProperty::DisplayName:
            return m_displayName;
        case ProgramProperty::Version:
            return m_displayVersion;
        case ProgramProperty::Publisher:
            return m_publisher;
        case ProgramProperty::InstallLocation:
            return m_installLocation;
        case ProgramProperty::UninstallString:
            return m_uninstallString;
        case ProgramProperty::InstallDate:
            return m_installDate;
        case ProgramProperty::EstimatedSize:
            return m_estimatedSize;
        case ProgramProperty::Comments:
            return m_comments;
        case ProgramProperty::HelpLink:
            return m_helpLink;
        case ProgramProperty::URLInfoAbout:
            return m_urlInfoAbout;
        default:
            return "";
    }
}

bool InstalledProgramInfo::MatchesFilter(const std::string& filter) const {
    if (filter.empty()) return true;

    // filter is pre-lowercased by caller
    if (pserv::utils::ToLower(m_displayName).find(filter) != std::string::npos) return true;
    if (pserv::utils::ToLower(m_publisher).find(filter) != std::string::npos) return true;

    return false;
}

} // namespace pserv