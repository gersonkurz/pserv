#include "precomp.h"
#include "console_table.h"
#include "console.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace pserv
{
    namespace console
    {
        ConsoleTable::ConsoleTable(const DataController *controller, OutputFormat format)
            : m_controller(controller), m_columns(controller->GetColumns()), m_format(format)
        {
        }

        void ConsoleTable::Render(const DataObjectContainer &objects)
        {
            // Handle non-table formats
            if (m_format == OutputFormat::Json)
            {
                RenderAsJson(objects);
                return;
            }
            else if (m_format == OutputFormat::Csv)
            {
                RenderAsCsv(objects);
                return;
            }

            // Calculate column widths based on content
            CalculateColumnWidths(objects);

            // Render table header
            RenderHeader();
            RenderSeparator();

            // Render each row
            for (const auto *obj : objects)
            {
                RenderRow(obj);
            }

            // Summary
            write_line(std::format("\n{} {} found", objects.GetSize(), m_controller->GetItemName()));
        }

        void ConsoleTable::CalculateColumnWidths(const DataObjectContainer &objects)
        {
            m_columnWidths.clear();
            m_columnWidths.reserve(m_columns.size());

            // Start with header widths
            for (const auto &col : m_columns)
            {
                m_columnWidths.push_back(col.DisplayName.length());
            }

            // Update with content widths (sample first 100 rows for performance)
            size_t rowCount = 0;
            for (const auto *obj : objects)
            {
                for (size_t i = 0; i < m_columns.size(); ++i)
                {
                    std::string value = obj->GetProperty(static_cast<int>(i));
                    m_columnWidths[i] = std::max(m_columnWidths[i], value.length());
                }

                // Stop sampling after 100 rows
                if (++rowCount >= 100)
                    break;
            }

            // Apply reasonable limits (min 3, max 50 chars per column)
            for (auto &width : m_columnWidths)
            {
                width = std::max<size_t>(3, std::min<size_t>(50, width));
            }
        }

        void ConsoleTable::RenderHeader()
        {
            write(CONSOLE_FOREGROUND_CYAN);
            for (size_t i = 0; i < m_columns.size(); ++i)
            {
                if (i > 0)
                    write("  ");
                write(FormatCell(m_columns[i].DisplayName, m_columnWidths[i], m_columns[i].GetAlignment()));
            }
            write_line(CONSOLE_STANDARD);
        }

        void ConsoleTable::RenderSeparator()
        {
            for (size_t i = 0; i < m_columns.size(); ++i)
            {
                if (i > 0)
                    write("  ");
                write(std::string(m_columnWidths[i], '-'));
            }
            write_line("");
        }

        void ConsoleTable::RenderRow(const DataObject *obj)
        {
            VisualState state = m_controller->GetVisualState(obj);
            const char *colorCode = GetColorForState(state);

            write(colorCode);
            for (size_t i = 0; i < m_columns.size(); ++i)
            {
                if (i > 0)
                    write("  ");
                std::string value = obj->GetProperty(static_cast<int>(i));
                write(FormatCell(value, m_columnWidths[i], m_columns[i].GetAlignment()));
            }
            write_line(CONSOLE_STANDARD);
        }

        std::string ConsoleTable::FormatCell(const std::string &value, size_t width, ColumnAlignment alignment) const
        {
            // Truncate if too long
            std::string displayValue = value;
            if (displayValue.length() > width)
            {
                displayValue = displayValue.substr(0, width - 3) + "...";
            }

            // Pad to width
            if (displayValue.length() < width)
            {
                size_t padding = width - displayValue.length();
                if (alignment == ColumnAlignment::Right)
                {
                    displayValue = std::string(padding, ' ') + displayValue;
                }
                else
                {
                    displayValue += std::string(padding, ' ');
                }
            }

            return displayValue;
        }

        const char *ConsoleTable::GetColorForState(VisualState state) const
        {
            switch (state)
            {
            case VisualState::Highlighted:
                return CONSOLE_FOREGROUND_GREEN;
            case VisualState::Disabled:
                return CONSOLE_FOREGROUND_GRAY;
            case VisualState::Normal:
            default:
                return CONSOLE_STANDARD;
            }
        }

        void ConsoleTable::RenderAsJson(const DataObjectContainer &objects)
        {
            write_line("{");
            write_line(std::format("  \"controller\": \"{}\",", m_controller->GetControllerName()));
            write_line(std::format("  \"item_type\": \"{}\",", m_controller->GetItemName()));
            write_line(std::format("  \"count\": {},", objects.GetSize()));
            write_line("  \"items\": [");

            bool first = true;
            for (const auto *obj : objects)
            {
                if (!first)
                    write_line(",");
                first = false;

                write("    {");
                bool firstProp = true;
                for (size_t i = 0; i < m_columns.size(); ++i)
                {
                    if (!firstProp)
                        write(", ");
                    firstProp = false;

                    std::string value = obj->GetProperty(static_cast<int>(i));
                    write(std::format("\"{}\": \"{}\"", m_columns[i].BindingName, JsonEscape(value)));
                }
                write("}");
            }

            write_line("");
            write_line("  ]");
            write_line("}");
        }

        void ConsoleTable::RenderAsCsv(const DataObjectContainer &objects)
        {
            // Header row
            for (size_t i = 0; i < m_columns.size(); ++i)
            {
                if (i > 0)
                    write(",");
                write(CsvEscape(m_columns[i].DisplayName));
            }
            write_line("");

            // Data rows
            for (const auto *obj : objects)
            {
                for (size_t i = 0; i < m_columns.size(); ++i)
                {
                    if (i > 0)
                        write(",");
                    std::string value = obj->GetProperty(static_cast<int>(i));
                    write(CsvEscape(value));
                }
                write_line("");
            }
        }

        std::string ConsoleTable::JsonEscape(const std::string &str) const
        {
            std::string escaped;
            escaped.reserve(str.length());
            for (char c : str)
            {
                switch (c)
                {
                case '"':
                    escaped += "\\\"";
                    break;
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    if (c < 0x20)
                    {
                        escaped += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                    }
                    else
                    {
                        escaped += c;
                    }
                    break;
                }
            }
            return escaped;
        }

        std::string ConsoleTable::CsvEscape(const std::string &str) const
        {
            // CSV escaping: wrap in quotes if contains comma, quote, or newline
            bool needsQuoting = str.find_first_of(",\"\n\r") != std::string::npos;

            if (!needsQuoting)
                return str;

            std::string escaped = "\"";
            for (char c : str)
            {
                if (c == '"')
                    escaped += "\"\""; // Double quotes are escaped by doubling
                else
                    escaped += c;
            }
            escaped += "\"";
            return escaped;
        }
    } // namespace console
} // namespace pserv
