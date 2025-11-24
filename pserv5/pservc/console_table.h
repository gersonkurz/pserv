#pragma once
#include <core/data_object.h>
#include <core/data_object_column.h>
#include <core/data_object_container.h>
#include <core/data_controller.h>

namespace pserv
{
    namespace console
    {
        // Output format for table rendering
        enum class OutputFormat
        {
            Table, // Formatted table with colors
            Json,  // JSON output
            Csv    // CSV output
        };

        // Table renderer for DataObject collections
        class ConsoleTable
        {
        private:
            const DataController *m_controller;
            const std::vector<DataObjectColumn> &m_columns;
            std::vector<size_t> m_columnWidths;
            OutputFormat m_format;

        public:
            ConsoleTable(const DataController *controller, OutputFormat format = OutputFormat::Table);

            // Render the entire table (headers + all rows)
            void Render(const DataObjectContainer &objects);

        private:
            // Calculate optimal column widths based on content
            void CalculateColumnWidths(const DataObjectContainer &objects);

            // Render table header
            void RenderHeader();

            // Render a single data row
            void RenderRow(const DataObject *obj);

            // Render table separator line
            void RenderSeparator();

            // Format a cell value with alignment
            std::string FormatCell(const std::string &value, size_t width, ColumnAlignment alignment) const;

            // Get ANSI color code based on visual state
            const char *GetColorForState(VisualState state) const;

            // Render as JSON
            void RenderAsJson(const DataObjectContainer &objects);

            // Render as CSV
            void RenderAsCsv(const DataObjectContainer &objects);

            // Escape string for JSON output
            std::string JsonEscape(const std::string &str) const;

            // Escape string for CSV output
            std::string CsvEscape(const std::string &str) const;
        };
    } // namespace console
} // namespace pserv
