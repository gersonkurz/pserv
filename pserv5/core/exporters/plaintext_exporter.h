#pragma once
#include "exporter_interface.h"

namespace pserv {

/**
 * Plain text exporter for human-readable output.
 * Exports DataObjects as "DisplayName: Value" lines.
 */
class PlainTextExporter : public IExporter {
public:
    std::string ExportSingle(
        const DataObject* object,
        const std::vector<DataObjectColumn>& columns) const override;

    std::string ExportMultiple(
        const std::vector<const DataObject*>& objects,
        const std::vector<DataObjectColumn>& columns) const override;

    std::string GetFormatName() const override { return "Plain Text"; }
    std::string GetFileExtension() const override { return ".txt"; }
};

} // namespace pserv
