#pragma once
#include "exporter_interface.h"

namespace pserv {

/**
 * JSON exporter using RapidJSON library.
 * Exports DataObjects as pretty-printed JSON objects or arrays.
 */
class JsonExporter : public IExporter {
public:
    std::string ExportSingle(
        const DataObject* object,
        const std::vector<DataObjectColumn>& columns) const override;

    std::string ExportMultiple(
        const std::vector<const DataObject*>& objects,
        const std::vector<DataObjectColumn>& columns) const override;

    std::string GetFormatName() const override { return "JSON"; }
    std::string GetFileExtension() const override { return ".json"; }
};

} // namespace pserv
