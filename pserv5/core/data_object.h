/// @file data_object.h
/// @brief Base class for all displayable data items (services, processes, etc.).
///
/// DataObject is the abstract base for all items shown in the main data grid.
/// Each concrete implementation (ServiceInfo, ProcessInfo, etc.) represents
/// a specific type of system object.
#pragma once
#include <core/refcount_interface.h>

namespace pserv
{
    /// @brief Type-safe property value for sorting and comparison.
    ///
    /// Properties can be empty (monostate), numeric (int64/uint64), or text (string).
    /// The variant type enables type-aware sorting in the grid.
    using PropertyValue = std::variant<std::monostate, int64_t, uint64_t, std::string>;

    /// @brief Abstract base class for all data items displayed in the UI.
    ///
    /// DataObject provides the interface for displaying, filtering, and sorting
    /// items in the data grid. Derived classes implement the specifics for each
    /// data type (services, processes, devices, etc.).
    ///
    /// @note DataObjects are reference-counted and managed by DataObjectContainer.
    class DataObject : public RefCountImpl
    {
    private:
        friend class DataObjectContainer;
        uint64_t m_LastSeenGeneration{0}; ///< Used for stale object detection during refresh.
        bool m_bIsRunning{false};         ///< Visual state hint: item is active/running.
        bool m_bIsDisabled{false};        ///< Visual state hint: item is disabled/inactive.

    protected:
        /// @brief Set the running state (affects visual highlighting).
        void SetRunning(bool bRunning) { m_bIsRunning = bRunning; }

        /// @brief Set the disabled state (affects visual graying).
        void SetDisabled(bool bDisabled) { m_bIsDisabled = bDisabled; }

    public:
        virtual ~DataObject() = default;

        /// @brief Get a property value as a display string.
        /// @param propertyId Column index (0-based).
        /// @return Formatted string for display in the grid.
        virtual std::string GetProperty(int propertyId) const = 0;

        /// @brief Get a property value with its native type for sorting.
        /// @param propertyId Column index (0-based).
        /// @return PropertyValue variant containing the typed value.
        virtual PropertyValue GetTypedProperty(int propertyId) const = 0;

        /// @brief Test if this object matches a filter string.
        /// @param filter Lowercase filter text to match against.
        /// @return true if any property contains the filter text.
        /// @note The filter is pre-lowercased by the caller for performance.
        virtual bool MatchesFilter(const std::string &filter) const = 0;

        /// @brief Get a human-readable name for this item.
        /// @return Display name (e.g., service name, process name).
        virtual std::string GetItemName() const = 0;

        /// @brief Check if this item is in a "running" state.
        bool IsRunning() const { return m_bIsRunning; }

        /// @brief Check if this item is disabled.
        bool IsDisabled() const { return m_bIsDisabled; }
    };
} // namespace pserv
