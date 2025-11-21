#include "precomp.h"
#include <core/exporters/json_exporter.h>
#include <core/exporters/exporter_registry.h>

namespace pserv {

namespace {
    // Static registration - runs during static initialization
    struct JsonExporterRegistration {
        JsonExporterRegistration() {
            ExporterRegistry::Instance().RegisterExporter(new JsonExporter());
        }
    };
    static JsonExporterRegistration g_jsonExporterRegistration;
}

std::string JsonExporter::ExportSingle(
    const DataObject* object,
    const std::vector<DataObjectColumn>& columns) const
{
    if (!object) return "{}";

    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    // Export each column as a property
    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& column = columns[i];
        const auto propertyName = column.BindingName;
        const auto propertyValue = object->GetProperty(static_cast<int>(i));

        // Add property to JSON object
        rapidjson::Value key{ propertyName.c_str(),
            static_cast<rapidjson::SizeType>(propertyName.size()), allocator };
        rapidjson::Value value{ propertyValue.c_str(),
            static_cast<rapidjson::SizeType>(propertyValue.size()), allocator };

        doc.AddMember(key, value, allocator);
    }

    // Convert to pretty-printed JSON string
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{ buffer };
    writer.SetIndent(' ', 2); // 2-space indentation
    doc.Accept(writer);

    return std::string{ buffer.GetString(), buffer.GetSize() };
}

std::string JsonExporter::ExportMultiple(
    const std::vector<const DataObject*>& objects,
    const std::vector<DataObjectColumn>& columns) const
{
    if (objects.empty()) return "[]";

    rapidjson::Document doc;
    doc.SetArray();
    auto& allocator = doc.GetAllocator();

    // Export each object as a JSON object in the array
    for (const auto* object : objects) {
        if (!object) continue;

        rapidjson::Value objValue{ rapidjson::kObjectType };

        // Export each column as a property
        for (size_t i = 0; i < columns.size(); ++i) {
            const auto& column = columns[i];
            const auto propertyName = column.BindingName;
            const auto propertyValue = object->GetProperty(static_cast<int>(i));

            // Add property to object
            rapidjson::Value key{ propertyName.c_str(),
                static_cast<rapidjson::SizeType>(propertyName.size()), allocator };
            rapidjson::Value value{ propertyValue.c_str(),
                static_cast<rapidjson::SizeType>(propertyValue.size()), allocator };

            objValue.AddMember(key, value, allocator);
        }

        doc.PushBack(objValue, allocator);
    }

    // Convert to pretty-printed JSON string
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{ buffer };
    writer.SetIndent(' ', 2); // 2-space indentation
    doc.Accept(writer);

    return std::string{ buffer.GetString(), buffer.GetSize() };
}

} // namespace pserv
