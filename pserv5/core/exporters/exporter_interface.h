#pragma once
#include <core/data_object.h>
#include <core/data_object_column.h>

namespace pserv
{

    /**
     * Abstract interface for exporting DataObjects to various formats.
     * Implementations provide format-specific serialization (JSON, plaintext, CSV, etc.)
     */
    class IExporter
    {
    public:
        virtual ~IExporter() = default;

        /**
         * Export a single DataObject to a string in the exporter's format.
         * @param object The object to export
         * @param columns Column metadata defining which properties to export and their names
         * @return Formatted string representation of the object
         */
        virtual std::string ExportSingle(const DataObject *object, const std::vector<DataObjectColumn> &columns) const = 0;

        /**
         * Export multiple DataObjects to a string in the exporter's format.
         * @param objects The objects to export
         * @param columns Column metadata defining which properties to export and their names
         * @return Formatted string representation of all objects
         */
        virtual std::string ExportMultiple(const std::vector<DataObject *> &objects, const std::vector<DataObjectColumn> &columns) const = 0;

        /**
         * Get the human-readable format name for UI display.
         * @return Format name (e.g., "JSON", "Plain Text", "CSV")
         */
        virtual std::string GetFormatName() const = 0;

        /**
         * Get the file extension for this format (including the dot).
         * @return File extension (e.g., ".json", ".txt", ".csv")
         */
        virtual std::string GetFileExtension() const = 0;
    };

} // namespace pserv
