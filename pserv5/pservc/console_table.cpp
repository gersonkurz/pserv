#include "precomp.h"
#include "console_table.h"
#include "console.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utils/string_utils.h>

namespace pserv
{
    namespace console
    {
        // Calculate the visual length of a string, excluding escape sequences
        // Uses proper Unicode conversion and handles surrogate pairs
        static size_t GetVisualLength(const std::string &str)
        {
            if (str.empty())
                return 0;

            // Step 1: Strip escape sequences
            std::string stripped;
            stripped.reserve(str.length());

            for (size_t i = 0; i < str.length(); ++i)
            {
                if (str[i] == '\x1b' && i + 1 < str.length())
                {
                    // Custom console escape sequence: skip both bytes
                    i++; // Skip the next byte
                }
                else if (str[i] == '\x1b')
                {
                    // Lone \x1b at end
                }
                else
                {
                    stripped += str[i];
                }
            }

            // Step 2: Convert to UTF-16 and count actual characters (handling surrogate pairs)
            std::wstring wide = utils::Utf8ToWide(stripped);
            size_t visualLen = 0;

            for (size_t i = 0; i < wide.length(); ++i)
            {
                wchar_t c = wide[i];

                // Check if this is a high surrogate (0xD800-0xDBFF)
                if (c >= 0xD800 && c <= 0xDBFF && i + 1 < wide.length())
                {
                    wchar_t next = wide[i + 1];
                    // Check if next is a low surrogate (0xDC00-0xDFFF)
                    if (next >= 0xDC00 && next <= 0xDFFF)
                    {
                        // This is a surrogate pair - one visual character
                        visualLen++;
                        i++; // Skip the low surrogate
                        continue;
                    }
                }

                // Regular character (including unpaired surrogates, though those shouldn't happen)
                visualLen++;
            }

            spdlog::debug("GetVisualLength: str.length()={}, stripped.length()={}, wide.length()={}, visualLen={}",
                          str.length(), stripped.length(), wide.length(), visualLen);
            return visualLen;
        }

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

            // Start with header widths (headers don't have color codes)
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
                    // Use visual length to exclude ANSI codes
                    m_columnWidths[i] = std::max(m_columnWidths[i], GetVisualLength(value));
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
                std::string formatted = FormatCell(m_columns[i].DisplayName, m_columnWidths[i], m_columns[i].GetAlignment());
                spdlog::debug("RenderHeader col[{}]: name='{}', width={}, formatted.length()={}",
                              i, m_columns[i].DisplayName, m_columnWidths[i], formatted.length());
                write(formatted);
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

            // Build the entire line first to measure it
            std::string line;
            line += colorCode;
            for (size_t i = 0; i < m_columns.size(); ++i)
            {
                if (i > 0)
                    line += "  ";
                std::string value = obj->GetProperty(static_cast<int>(i));
                std::string formatted = FormatCell(value, m_columnWidths[i], m_columns[i].GetAlignment());
                spdlog::debug("RenderRow col[{}]: value='{}', width={}, formatted.length()={}, formatted[0-10]='{}'",
                              i, value.substr(0, 20), m_columnWidths[i], formatted.length(),
                              formatted.substr(0, std::min<size_t>(10, formatted.length())));
                line += formatted;
            }
            line += CONSOLE_STANDARD;

            // Log the total line length
            size_t visualLineLen = GetVisualLength(line);
            spdlog::debug("RenderRow complete: line.length()={}, visualLength={}", line.length(), visualLineLen);

            write_line(line);
        }

        std::string ConsoleTable::FormatCell(const std::string &value, size_t width, ColumnAlignment alignment) const
        {
            // Strip newlines and carriage returns (replace with space)
            std::string cleanedValue = value;
            for (char &c : cleanedValue)
            {
                if (c == '\n' || c == '\r')
                {
                    c = ' ';
                }
            }

            // Calculate visual length (excluding ANSI codes)
            size_t visualLen = GetVisualLength(cleanedValue);

            spdlog::debug("FormatCell: value.length()={}, visualLen={}, width={}", cleanedValue.length(), visualLen, width);

            // Truncate if too long (need to be careful with escape codes)
            std::string displayValue = cleanedValue;
            if (visualLen > width)
            {
                spdlog::debug("  Need to truncate: visualLen({}) > width({})", visualLen, width);

                // Find where to cut based on visual length (UTF-8 aware)
                size_t targetLen = width - 3; // Reserve 3 for "..."
                size_t actualLen = 0;
                size_t cutPos = 0;

                for (size_t i = 0; i < cleanedValue.length(); ++i)
                {
                    unsigned char c = static_cast<unsigned char>(cleanedValue[i]);

                    if (c == '\x1b' && i + 1 < cleanedValue.length())
                    {
                        // Custom console escape sequence: skip both bytes
                        spdlog::debug("    [{}] Skip escape sequence", i);
                        i++; // Skip the next byte
                    }
                    else if (c == '\x1b')
                    {
                        // Lone \x1b at end
                        spdlog::debug("    [{}] Lone escape at end", i);
                    }
                    else if ((c & 0x80) == 0)
                    {
                        // ASCII character
                        if (actualLen >= targetLen)
                        {
                            cutPos = i;
                            spdlog::debug("    [{}] Cut position found, actualLen={}, targetLen={}", i, actualLen, targetLen);
                            break;
                        }
                        actualLen++;
                    }
                    else if ((c & 0xE0) == 0xC0)
                    {
                        // 2-byte UTF-8 character
                        if (actualLen >= targetLen)
                        {
                            cutPos = i;
                            spdlog::debug("    [{}] Cut position found (2-byte UTF-8), actualLen={}, targetLen={}", i, actualLen, targetLen);
                            break;
                        }
                        actualLen++;
                        i++; // Skip continuation byte
                    }
                    else if ((c & 0xF0) == 0xE0)
                    {
                        // 3-byte UTF-8 character
                        if (actualLen >= targetLen)
                        {
                            cutPos = i;
                            spdlog::debug("    [{}] Cut position found (3-byte UTF-8), actualLen={}, targetLen={}", i, actualLen, targetLen);
                            break;
                        }
                        actualLen++;
                        i += 2; // Skip continuation bytes
                    }
                    else if ((c & 0xF8) == 0xF0)
                    {
                        // 4-byte UTF-8 character
                        if (actualLen >= targetLen)
                        {
                            cutPos = i;
                            spdlog::debug("    [{}] Cut position found (4-byte UTF-8), actualLen={}, targetLen={}", i, actualLen, targetLen);
                            break;
                        }
                        actualLen++;
                        i += 3; // Skip continuation bytes
                    }
                    else
                    {
                        // Continuation byte or invalid - just skip
                    }
                }
                if (cutPos > 0)
                {
                    displayValue = cleanedValue.substr(0, cutPos) + "...";
                    // Recalculate visual length after truncation and adding "..."
                    size_t oldVisualLen = visualLen;
                    visualLen = GetVisualLength(displayValue);
                    spdlog::debug("  After truncation: cutPos={}, displayValue.length()={}, oldVisualLen={}, newVisualLen={}",
                                  cutPos, displayValue.length(), oldVisualLen, visualLen);
                }
            }

            // Pad to width (using visual length)
            if (visualLen < width)
            {
                size_t padding = width - visualLen;
                spdlog::debug("  Padding: adding {} spaces (alignment={})", padding, alignment == ColumnAlignment::Right ? "Right" : "Left");
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
