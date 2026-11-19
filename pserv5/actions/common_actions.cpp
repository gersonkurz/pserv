#include "precomp.h"
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <core/exporters/exporter_interface.h>
#include <core/exporters/exporter_registry.h>
#include <utils/string_utils.h>

namespace pserv
{

    namespace
    {

        // ============================================================================
        // Base Export Action
        // ============================================================================

        class ExportAction : public DataAction
        {
        protected:
            std::string m_exporterName;
            bool m_bCopyToClipboard;

            ExportAction(std::string name, std::string exporterName, bool copyToClipboard)
                : DataAction{std::move(name), ActionVisibility::ContextMenu},
                  m_exporterName{std::move(exporterName)},
                  m_bCopyToClipboard{copyToClipboard}
            {
            }

        public:
            static constexpr const char *OUTPUT_PARAMETER = "output";

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            std::vector<ActionParameter> GetParameters(const DataController *controller) const override
            {
                if (m_bCopyToClipboard)
                    return {};

                const IExporter *exporter = ExporterRegistry::Instance().FindExporter(m_exporterName);
                ActionParameter output{OUTPUT_PARAMETER, "Export to file", ActionParameterKind::SaveFilePath};
                if (exporter)
                {
                    output.DefaultValue = std::format("export_{}{}", controller ? std::string{controller->GetControllerName()} : std::string{"data"},
                        exporter->GetFileExtension());
                    output.FileFilters.push_back({std::format("{} Files", exporter->GetFormatName()), std::format("*{}", exporter->GetFileExtension())});
                }
                output.FileFilters.push_back({"All Files", "*.*"});
                return {output};
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                if (ctx.m_selectedObjects.empty())
                {
                    spdlog::warn("ExportAction: No objects selected");
                    return;
                }

                IExporter *exporter = ExporterRegistry::Instance().FindExporter(m_exporterName);
                if (!exporter)
                {
                    throw std::runtime_error(std::format("Exporter '{}' is not available", m_exporterName));
                }

                // Get columns from controller
                if (!ctx.m_pController)
                {
                    spdlog::error("ExportAction: No controller in context");
                    return;
                }
                const auto &columns = ctx.m_pController->GetColumns();

                // Export data
                std::string exportedData;
                try
                {
                    if (ctx.m_selectedObjects.size() == 1)
                    {
                        exportedData = exporter->ExportSingle(ctx.m_selectedObjects[0], columns);
                    }
                    else
                    {
                        exportedData = exporter->ExportMultiple(ctx.m_selectedObjects, columns);
                    }
                }
                catch (const std::exception &e)
                {
                    throw std::runtime_error(std::format("Export failed: {}", e.what()));
                }

                if (m_bCopyToClipboard)
                {
                    utils::CopyToClipboard(exportedData); // throws on failure; reported by the frontend
                    spdlog::info("Copied {} object(s) as {} to clipboard", ctx.m_selectedObjects.size(), exporter->GetFormatName());
                    return;
                }

                // Export to file: the frontend collected the path before dispatch (GetParameters)
                const std::string outputPath = ctx.GetParameter(OUTPUT_PARAMETER);
                if (outputPath.empty())
                {
                    throw std::runtime_error("No output file given for export");
                }

                std::ofstream outFile{utils::Utf8ToWide(outputPath), std::ios::binary};
                if (outFile)
                {
                    outFile.write(exportedData.c_str(), exportedData.size());
                    outFile.close();
                }
                if (!outFile)
                {
                    throw std::runtime_error(std::format("Failed to write file '{}'", outputPath));
                }

                spdlog::info("Exported {} object(s) as {} to file: {}", ctx.m_selectedObjects.size(), exporter->GetFormatName(), outputPath);
            }
        };

        // ============================================================================
        // Concrete Export Actions
        // ============================================================================

        class CopyAsJsonAction final : public ExportAction
        {
        public:
            CopyAsJsonAction()
                : ExportAction{"Copy as JSON", "JSON", true}
            {
            }
        };

        class ExportToJsonAction final : public ExportAction
        {
        public:
            ExportToJsonAction()
                : ExportAction{"Export to JSON...", "JSON", false}
            {
            }
        };

        class CopyAsTextAction final : public ExportAction
        {
        public:
            CopyAsTextAction()
                : ExportAction{"Copy as Plain Text", "Plain Text", true}
            {
            }
        };

        class ExportToTextAction final : public ExportAction
        {
        public:
            ExportToTextAction()
                : ExportAction{"Export to Plain Text...", "Plain Text", false}
            {
            }
        };

        CopyAsJsonAction theCopyAsJsonAction;
        ExportToJsonAction theExportToJsonAction;
        CopyAsTextAction theCopyAsTextAction;
        ExportToTextAction theExportToTextAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    void AddCommonExportActions(std::vector<const DataAction *> &actions)
    {
        const auto &exporters = ExporterRegistry::Instance().GetExporters();
        if (exporters.empty())
        {
            return;
        }

        // Add separator first
        actions.push_back(&theDataActionSeparator);

        // Add Copy/Export action pairs for each registered exporter
        for (const auto *exporter : exporters)
        {
            if (exporter->GetFormatName() == "JSON")
            {
                actions.push_back(&theCopyAsJsonAction);
                actions.push_back(&theExportToJsonAction);
            }
            else if (exporter->GetFormatName() == "Plain Text")
            {
                actions.push_back(&theCopyAsTextAction);
                actions.push_back(&theExportToTextAction);
            }
        }

        actions.push_back(&theDataActionSeparator);
        actions.push_back(&theDataPropertiesAction);
    }

} // namespace pserv
