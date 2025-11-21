#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/exporters/exporter_registry.h>
#include <dialogs/data_properties_dialog.h>
#include <utils/file_dialogs.h>
#include <utils/string_utils.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{

    void DataController::Sort(int columnIndex, bool ascending)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
            return;

        // Remember last sort for re-applying after refresh
        m_lastSortColumn = columnIndex;
        m_lastSortAscending = ascending;

        auto &objects = const_cast<std::vector<DataObject *> &>(GetDataObjects());
        ColumnDataType dataType = m_columns[columnIndex].DataType;

        std::sort(objects.begin(),
            objects.end(),
            [columnIndex, ascending, dataType](const DataObject *a, const DataObject *b)
            {
                PropertyValue valA = a->GetTypedProperty(columnIndex);
                PropertyValue valB = b->GetTypedProperty(columnIndex);

                // Numeric comparison for Integer, UnsignedInteger, and Size types
                if (dataType == ColumnDataType::Integer || dataType == ColumnDataType::UnsignedInteger || dataType == ColumnDataType::Size)
                {

                    uint64_t numA = 0, numB = 0;

                    if (std::holds_alternative<uint64_t>(valA))
                    {
                        numA = std::get<uint64_t>(valA);
                    }
                    else if (std::holds_alternative<int64_t>(valA))
                    {
                        numA = static_cast<uint64_t>(std::get<int64_t>(valA));
                    }

                    if (std::holds_alternative<uint64_t>(valB))
                    {
                        numB = std::get<uint64_t>(valB);
                    }
                    else if (std::holds_alternative<int64_t>(valB))
                    {
                        numB = static_cast<uint64_t>(std::get<int64_t>(valB));
                    }

                    return ascending ? (numA < numB) : (numA > numB);
                }

                // String comparison for String and Time types
                std::string strA, strB;

                if (std::holds_alternative<std::string>(valA))
                {
                    strA = std::get<std::string>(valA);
                }
                else
                {
                    strA = a->GetProperty(columnIndex);
                }

                if (std::holds_alternative<std::string>(valB))
                {
                    strB = std::get<std::string>(valB);
                }
                else
                {
                    strB = b->GetProperty(columnIndex);
                }

                int cmp = strA.compare(strB);
                return ascending ? (cmp < 0) : (cmp > 0);
            });
    }

    void DataController::RenderPropertiesDialog()
    {
        // Render service properties dialog if open
        if (m_pPropertiesDialog)
        {
            const bool changesApplied = m_pPropertiesDialog->Render();
            if (changesApplied)
            {
                // Refresh services list to show updated data
                Refresh();
            }
        }
    }
    void DataController::Clear()
    {
        for (auto *mod : m_objects)
        {
            delete mod;
        }
        m_objects.clear();
        m_bLoaded = false;
    }

    void DataController::DispatchCommonAction(int action, DataActionDispatchContext &context)
    {
        if (context.m_selectedObjects.empty())
        {
            spdlog::warn("DispatchCommonAction: No objects selected");
            return;
        }

        // Determine which exporter to use based on action
        IExporter *exporter = nullptr;
        bool bCopyToClipboard = false;

        auto commonAction = static_cast<CommonAction>(action);
        switch (commonAction)
        {
        case CommonAction::CopyAsJson:
            exporter = ExporterRegistry::Instance().FindExporter("JSON");
            bCopyToClipboard = true;
            break;
        case CommonAction::ExportToJson:
            exporter = ExporterRegistry::Instance().FindExporter("JSON");
            bCopyToClipboard = false;
            break;
        case CommonAction::CopyAsTxt:
            exporter = ExporterRegistry::Instance().FindExporter("Plain Text");
            bCopyToClipboard = true;
            break;
        case CommonAction::ExportToTxt:
            exporter = ExporterRegistry::Instance().FindExporter("Plain Text");
            bCopyToClipboard = false;
            break;
        default:
            spdlog::warn("DispatchCommonAction: Unknown action {}", action);
            return;
        }

        if (!exporter)
        {
            spdlog::error("DispatchCommonAction: Exporter not found");
            MessageBoxA(context.m_hWnd, "Exporter not available", "Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Export data
        std::string exportedData;
        try
        {
            if (context.m_selectedObjects.size() == 1)
            {
                exportedData = exporter->ExportSingle(context.m_selectedObjects[0], m_columns);
            }
            else
            {
                exportedData = exporter->ExportMultiple(context.m_selectedObjects, m_columns);
            }
        }
        catch (const std::exception &e)
        {
            spdlog::error("Export failed: {}", e.what());
            MessageBoxA(context.m_hWnd, std::format("Export failed: {}", e.what()).c_str(), "Error", MB_OK | MB_ICONERROR);
            return;
        }

        if (bCopyToClipboard)
        {
            // Copy to clipboard
            ImGui::SetClipboardText(exportedData.c_str());
            spdlog::info("Copied {} object(s) as {} to clipboard", context.m_selectedObjects.size(), exporter->GetFormatName());
        }
        else
        {
            // Export to file
            std::wstring defaultFileName = utils::Utf8ToWide(std::format("export_{}", m_controllerName));

            std::vector<utils::FileTypeFilter> filters{
                {std::format(L"{} Files", utils::Utf8ToWide(exporter->GetFormatName())), std::format(L"*{}", utils::Utf8ToWide(exporter->GetFileExtension()))},
                {L"All Files", L"*.*"}};

            std::wstring filePath;
            if (utils::SaveFileDialog(context.m_hWnd, L"Export Data", defaultFileName, filters, 0, filePath))
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

                    spdlog::info(
                        "Exported {} object(s) as {} to file: {}", context.m_selectedObjects.size(), exporter->GetFormatName(), utils::WideToUtf8(filePath));

                    MessageBoxA(context.m_hWnd,
                        std::format("Successfully exported {} object(s) to file.", context.m_selectedObjects.size()).c_str(),
                        "Export Complete",
                        MB_OK | MB_ICONINFORMATION);
                }
                catch (const std::exception &e)
                {
                    spdlog::error("File export failed: {}", e.what());
                    MessageBoxA(context.m_hWnd, std::format("Failed to export to file: {}", e.what()).c_str(), "Export Error", MB_OK | MB_ICONERROR);
                }
            }
            else
            {
                spdlog::debug("Export cancelled by user");
            }
        }
    }

} // namespace pserv
