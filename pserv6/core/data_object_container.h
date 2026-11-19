/// @file data_object_container.h
/// @brief Container for managing collections of DataObjects with stable ID lookup.
///
/// DataObjectContainer provides efficient storage and retrieval of DataObjects
/// with support for generation-based stale detection during refresh cycles.
#pragma once

#include <core/data_object.h>
#include <core/data_object_column.h>

namespace pserv
{
    /// @brief Container that manages a collection of DataObject instances.
    ///
    /// This container provides:
    /// - O(1) lookup by stable ID via hash map
    /// - Ordered iteration via vector
    /// - Generation-based stale object detection for refresh cycles
    /// - Sorting by column with type-aware comparison
    ///
    /// @par Ownership Model:
    /// The container owns references to all DataObjects it contains. Objects
    /// are Retained on insertion and Released on removal or destruction.
    ///
    /// @par Refresh Cycle:
    /// @code
    /// container.StartRefresh();       // Increment generation counter
    /// // Add/update objects (marks them with current generation)
    /// container.FinishRefresh();      // Remove objects not seen this cycle
    /// @endcode
    class DataObjectContainer final
    {
    public:
        DataObjectContainer() = default;
        ~DataObjectContainer();

        /// @name Copy and Move Semantics
        /// @{
        DataObjectContainer(const DataObjectContainer &copySrc);
        DataObjectContainer &operator=(const DataObjectContainer &copySrc);
        DataObjectContainer(DataObjectContainer &&moveSr);
        DataObjectContainer &operator=(DataObjectContainer &&moveSrc);
        /// @}

    public:
        /// @brief Remove all objects from the container.
        /// Releases references to all contained DataObjects.
        void Clear();
        
        /// @brief Find an object by its stable identifier.
        /// @tparam T The expected concrete type (must derive from DataObject).
        /// @param stableId The unique identifier returned by DataObject::GetStableID().
        /// @return Pointer to the object if found, nullptr otherwise.
        /// @note Marks the found object as seen in the current generation.
        /// @note Does NOT call Retain(); caller does not gain ownership.
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

        /// @brief Add a new object to the container.
        /// @tparam T The concrete type (must derive from DataObject).
        /// @param dataObject The object to add (container takes ownership).
        /// @return Pointer to the stored object (may differ if duplicate ID exists).
        /// @note If an object with the same stable ID exists, the new object is
        ///       Released and the existing object is returned.
        /// @note Marks the object with the current generation for stale detection.
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

        /// @brief Get the number of objects in the container.
        auto GetSize() const
        {
            return m_vector.size();
        }

        /// @name Range-based Iteration Support
        /// @{
        auto begin() const noexcept { return m_vector.begin(); }
        auto end() const noexcept { return m_vector.end(); }
        auto begin() noexcept { return m_vector.begin(); }
        auto end() noexcept { return m_vector.end(); }
        /// @}

        /// @brief Begin a refresh cycle by incrementing the generation counter.
        /// Call this before updating objects during a refresh operation.
        void StartRefresh() noexcept;

        /// @brief Complete a refresh cycle by removing stale objects.
        /// Objects not accessed since StartRefresh() are considered stale
        /// and will be removed and Released.
        void FinishRefresh();

        /// @brief Sort objects by a column value.
        /// @param columnIndex The column index to sort by.
        /// @param ascending True for ascending order, false for descending.
        /// @param dataType The column's data type for type-aware comparison.
        void Sort(int columnIndex, bool ascending, ColumnDataType dataType);

    private:
        std::unordered_map<std::string, DataObject*> m_lookup; ///< O(1) lookup by stable ID.
        std::vector<DataObject *> m_vector;                    ///< Ordered storage for iteration.
        uint64_t m_LastSeenGeneration{0};                      ///< Current generation for stale detection.
    };

} // namespace pserv
