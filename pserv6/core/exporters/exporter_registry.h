#pragma once
#include <core/exporters/exporter_interface.h>

namespace pserv
{

    /**
     * Singleton registry for managing available IExporter implementations.
     * Built-in exporters are registered on first use, including static-library builds.
     */
    class ExporterRegistry
    {
    public:
        static ExporterRegistry &Instance();

        /**
         * Register an exporter implementation.
         * @param exporter Owned pointer to exporter (registry takes ownership)
         */
        void RegisterExporter(IExporter *exporter);

        /**
         * Get all registered exporters.
         * @return Vector of exporter pointers (registry retains ownership)
         */
        const std::vector<IExporter *> &GetExporters() const;

        /**
         * Find an exporter by format name.
         * @param formatName Case-sensitive format name (e.g., "JSON")
         * @return Exporter pointer or nullptr if not found
         */
        IExporter *FindExporter(const std::string &formatName) const;

    private:
        ExporterRegistry();
        ~ExporterRegistry();
        ExporterRegistry(const ExporterRegistry &) = delete;
        ExporterRegistry &operator=(const ExporterRegistry &) = delete;

        std::vector<IExporter *> m_exporters;
    };

} // namespace pserv
