#pragma once

namespace pserv
{

    // Column data type for proper sorting and formatting
    enum class ColumnDataType
    {
        String,          // Text data, left-aligned
        Integer,         // Signed integer, right-aligned
        UnsignedInteger, // Unsigned integer, right-aligned
        Size,            // Byte size (display as human-readable), right-aligned
        Time             // Time/duration (display as human-readable), right-aligned
    };

    // Column alignment (auto-derived from data type)
    enum class ColumnAlignment
    {
        Left,
        Right
    };

    // Column edit type (how should this column be edited in properties dialog)
    enum class ColumnEditType
    {
        None,          // Read-only, not editable
        Text,          // Single-line text input
        TextMultiline, // Multi-line text input
        Integer,       // Numeric input
        Combo          // Dropdown with predefined options
    };

    class DataObjectColumn final
    {
    public:
        const std::string DisplayName;
        const std::string BindingName;
        const ColumnDataType DataType = ColumnDataType::String;
        const bool Editable = false;
        const ColumnEditType EditType = ColumnEditType::None;

        // Auto-derive alignment from data type
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
