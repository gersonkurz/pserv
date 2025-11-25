/// @file module_info.h
/// @brief Data model for loaded module (DLL) information.
///
/// Contains ModuleInfo class representing a DLL or executable module
/// loaded in a process's address space.
#pragma once
#include <core/data_object.h>

namespace pserv
{
    /// @brief Column indices for module properties.
    enum class ModuleProperty
    {
        BaseAddress = 0,
        Size,
        Name,
        Path,
        ProcessId
    };

    /// @brief Data model representing a loaded module.
    ///
    /// Stores module information from Tool Help snapshots:
    /// - Identity: module name and full path
    /// - Memory: base address and size in memory
    /// - Context: owning process ID
    class ModuleInfo : public DataObject
    {
    public:
        ModuleInfo(uint32_t processId, const std::string &name);

        void SetValues(void *baseAddress, uint32_t size, const std::string &path);

        // DataObject interface
        std::string GetProperty(int column) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return std::format("{} ({})", GetProperty(static_cast<int>(ModuleProperty::Name)), GetProperty(static_cast<int>(ModuleProperty::ProcessId)));
        }

        static std::string GetStableID(uint32_t processId, const std::string &name)
        {
            return std::format("{}:{}", processId, name);
        }

        std::string GetStableID() const
        {
            return GetStableID(m_processId, m_name);
        }
        // Module-specific getters
        uint32_t GetProcessId() const
        {
            return m_processId;
        }
        void *GetBaseAddress() const
        {
            return m_baseAddress;
        }
        uint32_t GetSize() const
        {
            return m_size;
        }
        const std::string &GetName() const
        {
            return m_name;
        }
        const std::string &GetPath() const
        {
            return m_path;
        }

    private:
        uint32_t m_processId;
        void *m_baseAddress;
        uint32_t m_size;
        std::string m_name;
        std::string m_path;
    };

} // namespace pserv
