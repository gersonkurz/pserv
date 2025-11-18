#pragma once
#include "../core/data_object.h"
#include <string>
#include <cstdint>

namespace pserv {

class ModuleInfo : public DataObject {
public:
    ModuleInfo(
        uint32_t processId,
        void* baseAddress,
        uint32_t size,
        const std::string& name,
        const std::string& path
    );

    // DataObject interface
    std::string GetId() const override;
    void Update(const DataObject& other) override;
    std::string GetProperty(int column) const override;
    bool MatchesFilter(const std::string& filter) const override;

    // Module-specific getters
    uint32_t GetProcessId() const { return m_processId; }
    void* GetBaseAddress() const { return m_baseAddress; }
    uint32_t GetSize() const { return m_size; }
    const std::string& GetName() const { return m_name; }
    const std::string& GetPath() const { return m_path; }

private:
    uint32_t m_processId;
    void* m_baseAddress;
    uint32_t m_size;
    std::string m_name;
    std::string m_path;
};

} // namespace pserv
