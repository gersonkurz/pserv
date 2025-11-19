#pragma once
#include "../core/data_object.h"
#include <string>
#include <cstdint>

namespace pserv {

class InstalledProgramInfo : public DataObject {
public:
    InstalledProgramInfo(
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
        uint64_t estimatedSizeBytes = 0);

    // DataObject overrides
    std::string GetId() const override;
    void Update(const DataObject& other) override;
    std::string GetProperty(int columnIndex) const override;
    PropertyValue GetTypedProperty(int propertyId) const override;
    bool MatchesFilter(const std::string& filter) const override;

    // Getters
    const std::string& GetDisplayName() const { return m_displayName; }
    const std::string& GetDisplayVersion() const { return m_displayVersion; }
    const std::string& GetPublisher() const { return m_publisher; }
    const std::string& GetInstallLocation() const { return m_installLocation; }
    const std::string& GetUninstallString() const { return m_uninstallString; }
    const std::string& GetInstallDate() const { return m_installDate; }
    const std::string& GetEstimatedSize() const { return m_estimatedSize; }
    uint64_t GetEstimatedSizeBytes() const { return m_estimatedSizeBytes; }
    const std::string& GetComments() const { return m_comments; }
    const std::string& GetHelpLink() const { return m_helpLink; }
    const std::string& GetUrlInfoAbout() const { return m_urlInfoAbout; }

private:
    std::string m_displayName;
    std::string m_displayVersion;
    std::string m_publisher;
    std::string m_installLocation;
    std::string m_uninstallString;
    std::string m_installDate;
    std::string m_estimatedSize;
    uint64_t m_estimatedSizeBytes;
    std::string m_comments;
    std::string m_helpLink;
    std::string m_urlInfoAbout;
}; 

} // namespace pserv
