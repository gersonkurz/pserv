#pragma once
#include <string>

namespace pserv {

    // Column data type for proper sorting and formatting
    enum class ColumnDataType {
        String,           // Text data, left-aligned
        Integer,          // Signed integer, right-aligned
        UnsignedInteger,  // Unsigned integer, right-aligned
        Size,             // Byte size (display as human-readable), right-aligned
        Time              // Time/duration (display as human-readable), right-aligned
    };

    // Column alignment (auto-derived from data type)
    enum class ColumnAlignment {
        Left,
        Right
    };

    class DataObjectColumn final {
    public:
        const std::string DisplayName;
        const std::string BindingName;
        const ColumnDataType DataType = ColumnDataType::String;

        // Auto-derive alignment from data type
        ColumnAlignment GetAlignment() const {
            switch (DataType) {
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
}
