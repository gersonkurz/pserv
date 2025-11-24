#include "precomp.h"
#include <core/refcount_interface.h>
#include <core/data_object.h>
#include <core/data_object_container.h>
#include <utils/string_utils.h>


namespace pserv
{
    DataObjectContainer::~DataObjectContainer()
    {
        Clear();
    }

    void DataObjectContainer::Sort(int columnIndex, bool ascending, ColumnDataType dataType)
    {
        std::sort(m_vector.begin(),
            m_vector.end(),
            [columnIndex, ascending, dataType](const DataObject *a, const DataObject *b)
            {
                PropertyValue valA = a->GetTypedProperty(columnIndex);
                PropertyValue valB = b->GetTypedProperty(columnIndex);

                // Numeric comparison for Integer, UnsignedInteger, and Size types
                if (dataType == ColumnDataType::Integer || dataType == ColumnDataType::UnsignedInteger || dataType == ColumnDataType::Size)
                {

                    uint64_t numA = 0, numB = 0;

                    if (std::holds_alternative<uint64_t>(valA))
                    {
                        numA = std::get<uint64_t>(valA);
                    }
                    else if (std::holds_alternative<int64_t>(valA))
                    {
                        numA = static_cast<uint64_t>(std::get<int64_t>(valA));
                    }

                    if (std::holds_alternative<uint64_t>(valB))
                    {
                        numB = std::get<uint64_t>(valB);
                    }
                    else if (std::holds_alternative<int64_t>(valB))
                    {
                        numB = static_cast<uint64_t>(std::get<int64_t>(valB));
                    }

                    return ascending ? (numA < numB) : (numA > numB);
                }

                // String comparison for String and Time types
                std::string strA, strB;

                if (std::holds_alternative<std::string>(valA))
                {
                    strA = std::get<std::string>(valA);
                }
                else
                {
                    strA = a->GetProperty(columnIndex);
                }

                if (std::holds_alternative<std::string>(valB))
                {
                    strB = std::get<std::string>(valB);
                }
                else
                {
                    strB = b->GetProperty(columnIndex);
                }

                // Case-insensitive comparison using Windows CompareStringEx
                // This handles Unicode properly and is locale-aware
                // Convert UTF-8 strings to wide strings first
                std::wstring wideA = utils::Utf8ToWide(strA);
                std::wstring wideB = utils::Utf8ToWide(strB);

                int cmp = CompareStringEx(
                    LOCALE_NAME_USER_DEFAULT,
                    LINGUISTIC_IGNORECASE,
                    wideA.c_str(), static_cast<int>(wideA.length()),
                    wideB.c_str(), static_cast<int>(wideB.length()),
                    nullptr, nullptr, 0);

                // CompareStringEx returns: 1 (less), 2 (equal), 3 (greater), or 0 (error)
                if (cmp == 0)
                {
                    // Error case - fall back to simple comparison
                    cmp = strA.compare(strB);
                    return ascending ? (cmp < 0) : (cmp > 0);
                }

                // Convert CompareStringEx result (1/2/3) to comparison result
                return ascending ? (cmp == 1) : (cmp == 3);
            });
    }

    void DataObjectContainer::StartRefresh() noexcept
    {
        for (auto dataObject : m_vector)
        {
            dataObject->m_LastSeenGeneration = m_LastSeenGeneration;
        }
        InterlockedIncrement(&m_LastSeenGeneration);
    }

    void DataObjectContainer::FinishRefresh()
    {
        bool repeat = true;
        while (repeat)
        {
            repeat = false;
            uint32_t index = 0;
            for (auto dataObject : m_vector)
            {
                if (dataObject->m_LastSeenGeneration != m_LastSeenGeneration)
                {
                    // Not seen in this generation, remove it
                    const auto stableId{dataObject->GetStableID()};
                    m_lookup.erase(stableId);
                    m_vector.erase(m_vector.begin() + index);
                    dataObject->Release(REFCOUNT_DEBUG_ARGS);
                    repeat = true;
                    break;
                }
                ++index;
            }
        }
    }

    DataObjectContainer::DataObjectContainer(const DataObjectContainer& copySrc)
    {
        // Copy constructor
        for (const auto& [key, dataObject] : copySrc.m_lookup)
        {
            assert(dataObject != nullptr);
            dataObject->Retain(REFCOUNT_DEBUG_ARGS);
            m_lookup[key] = dataObject;
        }
        m_vector = copySrc.m_vector;
    }

    DataObjectContainer& DataObjectContainer::operator=(const DataObjectContainer& copySrc)
    {
        if (this != &copySrc)
        {
            Clear();
            for (const auto& [key, dataObject] : copySrc.m_lookup)
            {
                assert(dataObject != nullptr);
                dataObject->Retain(REFCOUNT_DEBUG_ARGS);
                m_lookup[key] = dataObject;
            }
            m_vector = copySrc.m_vector;
        }
        return *this;
    }

    DataObjectContainer::DataObjectContainer(DataObjectContainer&& moveSrc)
    {
        m_lookup = std::move(moveSrc.m_lookup);
        m_vector = std::move(moveSrc.m_vector);
        moveSrc.m_lookup.clear();
        moveSrc.m_vector.clear();
    }

    DataObjectContainer& DataObjectContainer::operator=(DataObjectContainer&& moveSrc)
    {
        if (this != &moveSrc)
        {
            Clear();
            m_lookup = std::move(moveSrc.m_lookup);
            m_vector = std::move(moveSrc.m_vector);
            moveSrc.m_lookup.clear();
            moveSrc.m_vector.clear();
        }
        return *this;
    }

    

    void DataObjectContainer::Clear()
    {
        m_lookup.clear();
        for (const auto dataObject : m_vector)
        {
            assert(dataObject != nullptr);
            dataObject->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_vector.clear();
    }
} // namespace pserv
