#pragma once
#include "IRefCounted.h"
#include <string>

namespace pserv {
    class DataObject : public RefCountImpl {
    private:
        bool m_bIsRunning{false};
        bool m_bIsDisabled{false};

    protected:
        void SetRunning(bool bRunning) { m_bIsRunning = bRunning; }
        void SetDisabled(bool bDisabled) { m_bIsDisabled = bDisabled; }

    public:
        virtual ~DataObject() = default;
        virtual std::string GetId() const = 0;
        virtual void Update(const DataObject& other) = 0;
        virtual std::string GetProperty(int propertyId) const = 0;

        bool IsRunning() const { return m_bIsRunning; }
        bool IsDisabled() const { return m_bIsDisabled; }
    };
}
