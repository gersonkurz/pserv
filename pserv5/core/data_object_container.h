#pragma once

#include <core/data_object.h>
#include <core/data_object_column.h>

namespace pserv
{
    class DataObjectContainer final
    {
    public:
        DataObjectContainer() = default; // Default constructor
        ~DataObjectContainer();

        DataObjectContainer(const DataObjectContainer &copySrc);
        DataObjectContainer &operator=(const DataObjectContainer &copySrc);
        DataObjectContainer(DataObjectContainer &&moveSr);
        DataObjectContainer &operator=(DataObjectContainer &&moveSrc);

    public:
        void Clear();
        
        template <typename T> T* GetByStableId(const std::string &stableId) const
        {
            const auto existingIt{m_lookup.find(stableId)};
            if (existingIt != m_lookup.end())
            {
                auto dataObject = existingIt->second;
                dataObject->m_LastSeenGeneration = m_LastSeenGeneration;
                // do NOT call retain(), so that managers can assume they own the reference they get
                return static_cast<T *>(dataObject);
            }
            return nullptr;
        }

        // fails if an object with the same stable ID already exists
        template <typename T> T* Append(T* dataObject)
        {
            if (dataObject == nullptr)
            {
                spdlog::error("Attempt to insert null data object into DataObjectContainer");
                return nullptr;
            }
            dataObject->m_LastSeenGeneration = m_LastSeenGeneration;
            const auto stableId{dataObject->GetStableID()};
            const auto existingIt{m_lookup.find(stableId)};
            if (existingIt != m_lookup.end())
            {
                spdlog::warn("Attempt to insert stable object with id {}, but it already exists in the container", stableId);
                dataObject->Release(REFCOUNT_DEBUG_ARGS);
                return static_cast<T*>(existingIt->second);
            }
            m_vector.push_back(dataObject);
            m_lookup[stableId] = dataObject;
            return dataObject;
        }

        auto GetSize() const
        {
            return m_vector.size();
        }
        
        auto begin() const noexcept
        {
            return m_vector.begin();
        }

        auto end() const noexcept
        {
            return m_vector.end();
        }
        
        auto begin() noexcept
        {
            return m_vector.begin();
        }

        auto end() noexcept
        {
            return m_vector.end();
        }

        void StartRefresh() noexcept;
        void FinishRefresh();
        void Sort(int columnIndex, bool ascending, ColumnDataType dataType);

    private:
        std::unordered_map<std::string, DataObject*> m_lookup;
        std::vector<DataObject *> m_vector;
        uint64_t m_LastSeenGeneration{0};
    };

} // namespace pserv
