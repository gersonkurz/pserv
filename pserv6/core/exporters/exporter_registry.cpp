#include "precomp.h"
#include <core/exporters/exporter_registry.h>
#include <core/exporters/json_exporter.h>
#include <core/exporters/plaintext_exporter.h>

namespace pserv
{
    ExporterRegistry::ExporterRegistry()
    {
        // Explicit references keep both implementations reachable when the core
        // is linked as a static library. Unreferenced static registrars may be discarded.
        auto json = std::make_unique<JsonExporter>();
        auto text = std::make_unique<PlainTextExporter>();
        m_exporters.reserve(2);
        m_exporters.push_back(json.release());
        m_exporters.push_back(text.release());
    }

    ExporterRegistry &ExporterRegistry::Instance()
    {
        static ExporterRegistry instance;
        return instance;
    }

    ExporterRegistry::~ExporterRegistry()
    {
        for (auto *exporter : m_exporters)
        {
            delete exporter;
        }
        m_exporters.clear();
    }

    void ExporterRegistry::RegisterExporter(IExporter *exporter)
    {
        if (exporter)
        {
            m_exporters.push_back(exporter);
        }
    }

    const std::vector<IExporter *> &ExporterRegistry::GetExporters() const
    {
        return m_exporters;
    }

    IExporter *ExporterRegistry::FindExporter(const std::string &formatName) const
    {
        auto it = std::find_if(m_exporters.begin(),
            m_exporters.end(),
            [&formatName](const IExporter *exporter)
            {
                return exporter->GetFormatName() == formatName;
            });

        return (it != m_exporters.end()) ? *it : nullptr;
    }

} // namespace pserv
