#pragma once
#include <core/refcount_interface.h>

namespace pserv
{
    // Type-safe property value for sorting and comparison
    using PropertyValue = std::variant<std::monostate, int64_t, uint64_t, std::string>;

    class DataObject : public RefCountImpl
    {
    private:
        bool m_bIsRunning{false};
        bool m_bIsDisabled{false};

    protected:
        void SetRunning(bool bRunning)
        {
            m_bIsRunning = bRunning;
        }
        void SetDisabled(bool bDisabled)
        {
            m_bIsDisabled = bDisabled;
        }

    public:
        virtual ~DataObject() = default;
        virtual std::string GetProperty(int propertyId) const = 0;
        virtual PropertyValue GetTypedProperty(int propertyId) const = 0;
        // Note: filter is expected to be pre-lowercased for performance (done once at call site)
        virtual bool MatchesFilter(const std::string &filter) const = 0;
        virtual std::string GetItemName() const = 0;

        bool IsRunning() const
        {
            return m_bIsRunning;
        }
        bool IsDisabled() const
        {
            return m_bIsDisabled;
        }
    };
} // namespace pserv
