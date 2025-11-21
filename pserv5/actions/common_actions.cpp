#include "precomp.h"
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <core/exporters/exporter_interface.h>
#include <core/exporters/exporter_registry.h>
#include <utils/file_dialogs.h>
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
            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
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
                    spdlog::error("ExportAction: Exporter '{}' not found", m_exporterName);
                    MessageBoxA(ctx.m_hWnd, "Exporter not available", "Error", MB_OK | MB_ICONERROR);
                    return;
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
                    spdlog::error("Export failed: {}", e.what());
                    MessageBoxA(ctx.m_hWnd, std::format("Export failed: {}", e.what()).c_str(), "Error", MB_OK | MB_ICONERROR);
                    return;
                }

                if (m_bCopyToClipboard)
                {
                    // Copy to clipboard
                    ImGui::SetClipboardText(exportedData.c_str());
                    spdlog::info("Copied {} object(s) as {} to clipboard", ctx.m_selectedObjects.size(), exporter->GetFormatName());
                }
                else
                {
                    // Export to file
                    std::wstring defaultFileName = utils::Utf8ToWide(std::format("export_{}", ctx.m_pController->GetControllerName()));

                    std::vector<utils::FileTypeFilter> filters{{std::format(L"{} Files", utils::Utf8ToWide(exporter->GetFormatName())),
                                                                   std::format(L"*{}", utils::Utf8ToWide(exporter->GetFileExtension()))},
                        {L"All Files", L"*.*"}};

                    std::wstring filePath;
                    if (utils::SaveFileDialog(ctx.m_hWnd, L"Export Data", defaultFileName, filters, 0, filePath))
                    {
                        // Write to file
                        try
                        {
                            std::ofstream outFile{filePath, std::ios::binary};
                            if (!outFile)
                            {
                                throw std::runtime_error("Failed to open file for writing");
                            }

                            outFile.write(exportedData.c_str(), exportedData.size());
                            outFile.close();

                            if (outFile.fail())
                            {
                                throw std::runtime_error("Failed to write data to file");
                            }

                            spdlog::info("Exported {} object(s) as {} to file: {}",
                                ctx.m_selectedObjects.size(),
                                exporter->GetFormatName(),
                                utils::WideToUtf8(filePath));
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error("File write failed: {}", e.what());
                            MessageBoxA(ctx.m_hWnd, std::format("Failed to write file: {}", e.what()).c_str(), "Error", MB_OK | MB_ICONERROR);
                        }
                    }
                }
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
            spdlog::debug("AddCommonExportActions: No exporters registered");
            return;
        }

        // Add separator first
        actions.push_back(&theDataActionSeparator);

        // Add Copy/Export action pairs for each registered exporter
        for (const auto *exporter : exporters)
        {
            spdlog::debug("AddCommonExportActions: Checking exporter '{}'", exporter->GetFormatName());
            if (exporter->GetFormatName() == "JSON")
            {
                actions.push_back(&theCopyAsJsonAction);
                actions.push_back(&theExportToJsonAction);
                spdlog::debug("AddCommonExportActions: Added JSON actions");
            }
            else if (exporter->GetFormatName() == "Plain Text")
            {
                actions.push_back(&theCopyAsTextAction);
                actions.push_back(&theExportToTextAction);
                spdlog::debug("AddCommonExportActions: Added Plain Text actions");
            }
        }

        actions.push_back(&theDataActionSeparator);
        actions.push_back(&theDataPropertiesAction);

        spdlog::debug("AddCommonExportActions: Created {} common actions", actions.size());
    }

} // namespace pserv
