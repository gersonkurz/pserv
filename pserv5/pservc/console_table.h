/// @file console_table.h
/// @brief Console table renderer for DataObject collections.
///
/// Provides formatted table, JSON, and CSV output of data controller
/// contents for the command-line interface.
#pragma once
#include <core/data_object.h>
#include <core/data_object_column.h>
#include <core/data_object_container.h>
#include <core/data_controller.h>
#include <map>

namespace pserv
{
    namespace console
    {
        /// @brief Output format for console rendering.
        enum class OutputFormat
        {
            Table, ///< Formatted ASCII table with colors.
            Json,  ///< JSON array output.
            Csv    ///< Comma-separated values.
        };

        /// @brief Table renderer for DataObject collections.
        ///
        /// Renders data controller contents to the console in various
        /// formats with support for filtering and column alignment.
        class ConsoleTable
        {
        private:
            const DataController *m_controller;
            const std::vector<DataObjectColumn> &m_columns;
            std::vector<size_t> m_columnWidths;
            OutputFormat m_format;

        public:
            ConsoleTable(const DataController *controller, OutputFormat format = OutputFormat::Table);

            /// @brief Render the data to console.
            /// @param objects Container of DataObjects to render.
            /// @param filter Optional case-insensitive substring filter (empty = no filter).
            /// @param columnFilters Map of column index -> filter value for column-specific filtering.
            void Render(const DataObjectContainer &objects, const std::string &filter = "", const std::map<int, std::string> &columnFilters = {});

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

            // Check if an object matches all filters
            bool ObjectMatchesFilters(const DataObject *obj, const std::string &lowerFilter, const std::map<int, std::string> &columnFilters) const;

            // Render as JSON
            void RenderAsJson(const DataObjectContainer &objects, const std::string &lowerFilter, const std::map<int, std::string> &columnFilters);

            // Render as CSV
            void RenderAsCsv(const DataObjectContainer &objects, const std::string &lowerFilter, const std::map<int, std::string> &columnFilters);

            // Escape string for JSON output
            std::string JsonEscape(const std::string &str) const;

            // Escape string for CSV output
            std::string CsvEscape(const std::string &str) const;
        };
    } // namespace console
} // namespace pserv
