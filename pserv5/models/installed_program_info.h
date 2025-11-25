/// @file installed_program_info.h
/// @brief Data model for installed program information.
///
/// Contains InstalledProgramInfo class representing an installed
/// application from the Windows registry uninstall keys.
#pragma once
#include <core/data_object.h>

namespace pserv
{
    /// @brief Column indices for installed program properties.
    enum class ProgramProperty
    {
        DisplayName = 0,
        Version,
        Publisher,
        InstallLocation,
        UninstallString,
        InstallDate,
        EstimatedSize,
        Comments,
        HelpLink,
        URLInfoAbout
    };

    /// @brief Data model representing an installed program.
    ///
    /// Stores program information from registry uninstall keys:
    /// - Identity: display name, version, publisher
    /// - Installation: location, date, size
    /// - Uninstall: uninstall command string
    /// - Links: help URL, about URL
    class InstalledProgramInfo : public DataObject
    {
    public:
        InstalledProgramInfo(std::string displayName, std::string displayVersion, std::string uninstallString);

        void SetValues(
            std::string publisher,
            std::string installLocation,
            std::string installDate,
            std::string estimatedSize,
            std::string comments,
            std::string helpLink,
            std::string urlInfoAbout,
            uint64_t estimatedSizeBytes = 0);

        // DataObject overrides
        std::string GetProperty(int columnIndex) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        
        static std::string GetStableID(const std::string &displayName, const std::string &displayVersion, const std::string& uninstallString)
        {
            return std::format("{}:{}:{}", displayName, displayVersion, uninstallString);
        }

        std::string GetStableID() const
        {
            return GetStableID(m_displayName, m_displayVersion, m_uninstallString);
        }

        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(ProgramProperty::DisplayName));
        }

        // Getters
        const std::string &GetDisplayName() const
        {
            return m_displayName;
        }
        const std::string &GetDisplayVersion() const
        {
            return m_displayVersion;
        }
        const std::string &GetPublisher() const
        {
            return m_publisher;
        }
        const std::string &GetInstallLocation() const
        {
            return m_installLocation;
        }
        const std::string &GetUninstallString() const
        {
            return m_uninstallString;
        }
        const std::string &GetInstallDate() const
        {
            return m_installDate;
        }
        const std::string &GetEstimatedSize() const
        {
            return m_estimatedSize;
        }
        uint64_t GetEstimatedSizeBytes() const
        {
            return m_estimatedSizeBytes;
        }
        const std::string &GetComments() const
        {
            return m_comments;
        }
        const std::string &GetHelpLink() const
        {
            return m_helpLink;
        }
        const std::string &GetUrlInfoAbout() const
        {
            return m_urlInfoAbout;
        }

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
