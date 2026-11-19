/// @file data_object_column.h
/// @brief Column metadata for data grid display and editing.
///
/// Defines column properties including data type, alignment, and edit behavior
/// for the properties dialog.
#pragma once

namespace pserv
{

    /// @brief Data type for a column, affects sorting and alignment.
    enum class ColumnDataType
    {
        String,          ///< Text data, left-aligned, lexicographic sort.
        Integer,         ///< Signed integer, right-aligned, numeric sort.
        UnsignedInteger, ///< Unsigned integer, right-aligned, numeric sort.
        Size,            ///< Byte size, displayed human-readable (KB/MB/GB).
        Time             ///< Time/duration, displayed human-readable.
    };

    /// @brief Column text alignment in the grid.
    enum class ColumnAlignment
    {
        Left,  ///< Left-aligned (default for text).
        Right  ///< Right-aligned (default for numbers).
    };

    /// @brief How a column can be edited in the properties dialog.
    enum class ColumnEditType
    {
        None,          ///< Read-only, not editable.
        Text,          ///< Single-line text input.
        TextMultiline, ///< Multi-line text area.
        Integer,       ///< Numeric input with validation.
        Combo          ///< Dropdown with predefined options.
    };

    /// @brief Metadata describing a single column in the data grid.
    ///
    /// Each DataController defines its columns as a vector of DataObjectColumn.
    /// The column metadata controls display formatting, sorting behavior,
    /// and edit capabilities in the properties dialog.
    class DataObjectColumn final
    {
    public:
        /// @brief Construct a column bound to a model property.
        /// @param propertyId The model's property enum value this column shows. Every consumer
        ///        addresses properties by column index, so a controller's columns must be listed
        ///        in property-id order: column i must name property i. DataController checks this.
        template <typename PropertyEnum>
            requires std::is_enum_v<PropertyEnum>
        DataObjectColumn(PropertyEnum propertyId, std::string displayName, std::string bindingName,
            ColumnDataType dataType = ColumnDataType::String, bool editable = false,
            ColumnEditType editType = ColumnEditType::None)
            : PropertyId{static_cast<int>(propertyId)}
            , DisplayName{std::move(displayName)}
            , BindingName{std::move(bindingName)}
            , DataType{dataType}
            , Editable{editable}
            , EditType{editType}
        {
        }

        const int PropertyId;            ///< Model property this column shows (must equal its index).
        const std::string DisplayName;   ///< Header text shown in the grid.
        const std::string BindingName;   ///< Internal name for CLI/config reference.
        const ColumnDataType DataType = ColumnDataType::String; ///< Data type for sorting/alignment.
        const bool Editable = false;     ///< Whether this column can be edited.
        const ColumnEditType EditType = ColumnEditType::None;   ///< Edit widget type.

        /// @brief Get the text alignment based on data type.
        /// @return Left for strings, Right for numeric types.
        ColumnAlignment GetAlignment() const
        {
            switch (DataType)
            {
            case ColumnDataType::String:
                return ColumnAlignment::Left;
            case ColumnDataType::Integer:
            case ColumnDataType::UnsignedInteger:
            case ColumnDataType::Size:
            case ColumnDataType::Time:
                return ColumnAlignment::Right;
            default:
                return ColumnAlignment::Left;
            }
        }
    };
} // namespace pserv
