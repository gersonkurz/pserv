#include "precomp.h"
#include <core/exporters/exporter_registry.h>
#include <core/data_object.h>
#include <iostream>

struct Example final : pserv::DataObject
{
    std::string GetStableID() const override { return "example"; }
    std::string GetProperty(int) const override { return "Grüße \"quoted\""; }
    pserv::PropertyValue GetTypedProperty(int) const override { return GetProperty(0); }
    bool MatchesFilter(const std::string&) const override { return true; }
    std::string GetItemName() const override { return "example"; }
};

int main() try
{
    // Deliberately reference only the registry, not either concrete exporter.
    // This catches static-library dead stripping of registration-only objects.
    auto& registry = pserv::ExporterRegistry::Instance();
    if (registry.GetExporters().size() != 2) throw std::runtime_error("Built-in exporters missing or duplicated");
    auto json = registry.FindExporter("JSON");
    auto text = registry.FindExporter("Plain Text");
    if (!json || !text) throw std::runtime_error("Exporter lookup failed after static linking");
    Example example;
    std::vector<pserv::DataObjectColumn> columns{{"Display name", "Name", pserv::ColumnDataType::String}};
    auto output = json->ExportSingle(&example, columns);
    rapidjson::Document parsed;
    parsed.Parse(output.c_str());
    if (parsed.HasParseError() || !parsed.IsObject() || !parsed.HasMember("Name") ||
        example.GetProperty(0) != parsed["Name"].GetString()) throw std::runtime_error("JSON export changed Unicode/escaping");
    if (text->ExportSingle(&example, columns) != "Display name: " + example.GetProperty(0) + "\n")
        throw std::runtime_error("Plain text export failed");
    if (&registry != &pserv::ExporterRegistry::Instance() || registry.GetExporters().size() != 2)
        throw std::runtime_error("Repeated lookup duplicated exporters");
    std::cout << "Static-library exporter registration and Unicode output checks passed.\n";
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
