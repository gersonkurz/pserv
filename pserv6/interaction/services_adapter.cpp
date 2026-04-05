#include <interaction/services_adapter.h>

#include <algorithm>

namespace pserv6::interaction
{
    namespace
    {
        std::vector<ColumnDescriptor> BuildColumns()
        {
            return {
                {"name", "Name", ColumnKind::Text, true, true, true, false, FieldEditor::ReadOnly},
                {"display_name", "Display Name", ColumnKind::Text, true, true, true, true, FieldEditor::Text},
                {"status", "Status", ColumnKind::Text, true, true, true, false, FieldEditor::ReadOnly},
                {"start_type", "Start Type", ColumnKind::Text, true, true, true, true, FieldEditor::Combo},
                {"pid", "PID", ColumnKind::UnsignedInteger, true, true, true, false, FieldEditor::ReadOnly},
            };
        }
    }

    ServicesViewAdapter::ServicesViewAdapter()
        : m_descriptor{
              "services",
              "Services",
              "Service",
              "Services",
              SelectionMode::Multiple,
              true,
              true,
              true,
              BuildColumns()}
    {
    }

    const ExplorerViewDescriptor &ServicesViewAdapter::Describe() const
    {
        return m_descriptor;
    }

    ViewRefreshResult ServicesViewAdapter::Refresh(const ExplorerViewState &, bool) 
    {
        ViewRefreshResult result;
        result.Items = {
            {"Spooler",
             {std::string{"Spooler"}, std::string{"Print Spooler"}, std::string{"Running"}, std::string{"Automatic"}, uint64_t{4120}},
             VisualState::Highlighted,
             false},
            {"BITS",
             {std::string{"BITS"}, std::string{"Background Intelligent Transfer Service"}, std::string{"Stopped"}, std::string{"Manual"}, uint64_t{0}},
             VisualState::Normal,
             false},
            {"W32Time",
             {std::string{"W32Time"}, std::string{"Windows Time"}, std::string{"Stopped"}, std::string{"Disabled"}, uint64_t{0}},
             VisualState::Disabled,
             false},
        };
        return result;
    }

    std::vector<ActionDescriptor> ServicesViewAdapter::GetActions(const ExplorerViewState &state) const
    {
        if (state.SelectedItemIds.empty())
        {
            return {};
        }

        return {
            {"refresh", "Refresh", false, false, true},
            {"start", "Start", false, false, true},
            {"stop", "Stop", true, true, true},
            {"properties", "Properties", false, false, true},
        };
    }

    std::optional<DetailDocument> ServicesViewAdapter::GetDetails(std::string_view itemId) const
    {
        DetailDocument doc;
        doc.ItemId = std::string(itemId);
        doc.Title = std::string(itemId);

        if (itemId == "Spooler")
        {
            doc.Sections = {
                {"identity",
                 "Identity",
                 {
                     {"name", "Name", std::string{"Spooler"}, FieldEditor::ReadOnly, false, true, {}},
                     {"display_name", "Display Name", std::string{"Print Spooler"}, FieldEditor::Text, true, true, {}},
                     {"status", "Status", std::string{"Running"}, FieldEditor::ReadOnly, false, false, {}},
                 }},
                {"configuration",
                 "Configuration",
                 {
                     {"start_type", "Start Type", std::string{"Automatic"}, FieldEditor::Combo, true, false,
                      {{"automatic", "Automatic"}, {"manual", "Manual"}, {"disabled", "Disabled"}}},
                     {"binary_path", "Binary Path", std::string{"C:\\Windows\\System32\\spoolsv.exe"}, FieldEditor::Text, true, false, {}},
                 }},
            };
            return doc;
        }

        if (itemId == "BITS")
        {
            doc.Sections = {
                {"identity",
                 "Identity",
                 {
                     {"name", "Name", std::string{"BITS"}, FieldEditor::ReadOnly, false, true, {}},
                     {"display_name", "Display Name", std::string{"Background Intelligent Transfer Service"}, FieldEditor::Text, true, true, {}},
                     {"status", "Status", std::string{"Stopped"}, FieldEditor::ReadOnly, false, false, {}},
                 }},
            };
            return doc;
        }

        if (itemId == "W32Time")
        {
            doc.Sections = {
                {"identity",
                 "Identity",
                 {
                     {"name", "Name", std::string{"W32Time"}, FieldEditor::ReadOnly, false, true, {}},
                     {"display_name", "Display Name", std::string{"Windows Time"}, FieldEditor::Text, true, true, {}},
                     {"status", "Status", std::string{"Stopped"}, FieldEditor::ReadOnly, false, false, {}},
                 }},
            };
            return doc;
        }

        return std::nullopt;
    }

    bool ServicesViewAdapter::ApplyDetailEdits(std::string_view, const DetailDocument &, std::string &)
    {
        return true;
    }

    bool ServicesViewAdapter::SetRemoteContext(const RemoteContext &context, std::string &)
    {
        m_remoteContext = context;
        return true;
    }

    RemoteContext ServicesViewAdapter::GetRemoteContext() const
    {
        return m_remoteContext;
    }

    ActionResult ServicesActionExecutor::Execute(const ActionRequest &request, IActionExecutionHost &host)
    {
        ActionResult result;

        if (request.ActionId == "stop")
        {
            if (!host.Confirm({"Stop Service", "This is still a stubbed action, but the confirmation flow is real.", true}))
            {
                result.Cancelled = true;
                return result;
            }
        }

        host.ReportProgress({0.25f, "Preparing action..."});
        if (host.IsCancellationRequested())
        {
            result.Cancelled = true;
            return result;
        }

        host.ReportProgress({0.75f, "Executing stub action..."});
        if (host.IsCancellationRequested())
        {
            result.Cancelled = true;
            return result;
        }

        result.Success = true;
        result.RequiresRefresh = request.ActionId != "properties";
        return result;
    }
} // namespace pserv6::interaction

