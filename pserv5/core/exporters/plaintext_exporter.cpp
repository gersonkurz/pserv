#include "precomp.h"
#include <core/exporters/exporter_registry.h>
#include <core/exporters/plaintext_exporter.h>

namespace pserv
{

    namespace
    {
        // Static registration - runs during static initialization
        struct PlainTextExporterRegistration
        {
            PlainTextExporterRegistration()
            {
                ExporterRegistry::Instance().RegisterExporter(new PlainTextExporter());
            }
        };
        static PlainTextExporterRegistration g_plainTextExporterRegistration;
    } // namespace

    std::string PlainTextExporter::ExportSingle(const DataObject *object, const std::vector<DataObjectColumn> &columns) const
    {
        if (!object)
            return "";

        std::ostringstream oss;

        // Export each column as "DisplayName: Value\n"
        for (size_t i = 0; i < columns.size(); ++i)
        {
            const auto &column = columns[i];
            const auto propertyValue = object->GetProperty(static_cast<int>(i));

            oss << column.DisplayName << ": " << propertyValue << "\n";
        }

        return oss.str();
    }

    std::string PlainTextExporter::ExportMultiple(const std::vector<DataObject *> &objects, const std::vector<DataObjectColumn> &columns) const
    {
        if (objects.empty())
            return "";

        std::ostringstream oss;

        // Add header with count
        oss << "Exported " << objects.size() << " object(s)\n";
        oss << "-----------------------------------\n\n";

        // Export each object with separator
        for (size_t objIndex = 0; objIndex < objects.size(); ++objIndex)
        {
            const auto *object = objects[objIndex];
            if (!object)
                continue;

            // Add object number for multiple objects
            if (objects.size() > 1)
            {
                oss << "Object " << (objIndex + 1) << ":\n";
            }

            // Export properties
            for (size_t i = 0; i < columns.size(); ++i)
            {
                const auto &column = columns[i];
                const auto propertyValue = object->GetProperty(static_cast<int>(i));

                oss << column.DisplayName << ": " << propertyValue << "\n";
            }

            // Add blank line between objects
            if (objIndex < objects.size() - 1)
            {
                oss << "\n";
            }
        }

        return oss.str();
    }

} // namespace pserv
