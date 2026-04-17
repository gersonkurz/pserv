#include <interaction/services_adapter.h>

#include <controllers/services_data_controller.h>
#include <core/async_operation.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <models/service_info.h>
#include <utils/string_utils.h>

#include <algorithm>
#include <format>
#include <ranges>
#include <unordered_map>

namespace pserv6::interaction
{
    namespace
    {
        ColumnKind ToColumnKind(pserv::ColumnDataType type)
        {
            switch (type)
            {
            case pserv::ColumnDataType::Integer:
                return ColumnKind::Integer;
            case pserv::ColumnDataType::UnsignedInteger:
                return ColumnKind::UnsignedInteger;
            case pserv::ColumnDataType::Size:
                return ColumnKind::Size;
            case pserv::ColumnDataType::Time:
                return ColumnKind::Time;
            case pserv::ColumnDataType::String:
            default:
                return ColumnKind::Text;
            }
        }

        FieldEditor ToFieldEditor(pserv::ColumnEditType type)
        {
            switch (type)
            {
            case pserv::ColumnEditType::Text:
                return FieldEditor::Text;
            case pserv::ColumnEditType::TextMultiline:
                return FieldEditor::MultilineText;
            case pserv::ColumnEditType::Combo:
                return FieldEditor::Combo;
            case pserv::ColumnEditType::Integer:
                return FieldEditor::Text;
            case pserv::ColumnEditType::None:
            default:
                return FieldEditor::ReadOnly;
            }
        }

        VisualState ToVisualState(pserv::VisualState state)
        {
            switch (state)
            {
            case pserv::VisualState::Highlighted:
                return VisualState::Highlighted;
            case pserv::VisualState::Disabled:
                return VisualState::Disabled;
            case pserv::VisualState::Normal:
            default:
                return VisualState::Normal;
            }
        }

        Value ToValue(const pserv::PropertyValue &value)
        {
            return std::visit(
                [](const auto &v) -> Value
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>)
                    {
                        return std::monostate{};
                    }
                    else if constexpr (std::is_same_v<T, int64_t>)
                    {
                        return v;
                    }
                    else if constexpr (std::is_same_v<T, uint64_t>)
                    {
                        return v;
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        return v;
                    }
                    else
                    {
                        return std::string{};
                    }
                },
                value);
        }

        std::string ValueToString(const Value &value)
        {
            return std::visit(
                [](const auto &v) -> std::string
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::monostate>)
                    {
                        return {};
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        return v ? "true" : "false";
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        return v;
                    }
                    else
                    {
                        return std::to_string(v);
                    }
                },
                value);
        }

        std::string NormalizeActionId(std::string label)
        {
            label = pserv::utils::ToLower(label);
            for (char &c : label)
            {
                if (c == ' ' || c == ':' || c == '.' || c == '/')
                {
                    c = '-';
                }
            }

            label.erase(std::remove(label.begin(), label.end(), '\''), label.end());
            while (label.find("--") != std::string::npos)
            {
                label.replace(label.find("--"), 2, "-");
            }
            return label;
        }

        const pserv::DataObject *FindObject(const pserv::DataController &controller, std::string_view itemId)
        {
            for (const auto *obj : controller.GetDataObjects())
            {
                if (obj && obj->GetStableID() == itemId)
                {
                    return obj;
                }
            }
            return nullptr;
        }

        pserv::DataObject *FindObjectMutable(pserv::DataController &controller, std::string_view itemId)
        {
            for (auto *obj : controller.GetDataObjects())
            {
                if (obj && obj->GetStableID() == itemId)
                {
                    return obj;
                }
            }
            return nullptr;
        }
    }

    struct ServicesViewAdapter::Impl
    {
        std::unique_ptr<pserv::DataController> Controller;
        ExplorerViewDescriptor Descriptor;
        RemoteContext Context;

        static bool IsDefaultVisibleColumn(std::string_view columnId)
        {
            return columnId == "Name" ||
                   columnId == "DisplayName" ||
                   columnId == "Status" ||
                   columnId == "StartType" ||
                   columnId == "ProcessId" ||
                   columnId == "User";
        }

        Impl()
            : Controller{std::unique_ptr<pserv::DataController>(new pserv::ServicesDataController{})}
        {
            Descriptor.Id = "services";
            Descriptor.Title = "Services";
            Descriptor.ItemNameSingular = "Service";
            Descriptor.ItemNamePlural = "Services";
            Descriptor.Selection = SelectionMode::Multiple;
            Descriptor.SupportsAutoRefresh = Controller->SupportsAutoRefresh();
            Descriptor.SupportsDetails = true;
            Descriptor.SupportsRemoteContext = true;

            for (const auto &column : Controller->GetColumns())
            {
                Descriptor.Columns.push_back(ColumnDescriptor{
                    column.BindingName,
                    column.DisplayName,
                    ToColumnKind(column.DataType),
                    true,
                    true,
                    IsDefaultVisibleColumn(column.BindingName),
                    column.Editable,
                    ToFieldEditor(column.EditType)});
            }
        }

        pserv::ServicesDataController &ServicesController()
        {
            return static_cast<pserv::ServicesDataController &>(*Controller);
        }
    };

    struct ServicesActionExecutor::Impl
    {
        ServicesViewAdapter &Adapter;
        explicit Impl(ServicesViewAdapter &adapter)
            : Adapter(adapter)
        {
        }
    };

    ServicesViewAdapter::ServicesViewAdapter()
        : m_impl{std::make_unique<Impl>()}
    {
    }

    ServicesViewAdapter::~ServicesViewAdapter() = default;

    const ExplorerViewDescriptor &ServicesViewAdapter::Describe() const
    {
        return m_impl->Descriptor;
    }

    ViewRefreshResult ServicesViewAdapter::Refresh(const ExplorerViewState &state, bool isAutoRefresh)
    {
        ViewRefreshResult result;

        m_impl->Controller->Refresh(isAutoRefresh);

        if (state.Sort.has_value())
        {
            const auto it = std::ranges::find_if(
                m_impl->Descriptor.Columns,
                [&state](const ColumnDescriptor &column)
                {
                    return column.Id == state.Sort->ColumnId;
                });
            if (it != m_impl->Descriptor.Columns.end())
            {
                const auto index = static_cast<int>(std::distance(m_impl->Descriptor.Columns.begin(), it));
                m_impl->Controller->Sort(index, state.Sort->Ascending);
            }
        }

        const std::string lowerFilter = pserv::utils::ToLower(state.Filter.GlobalText);
        std::unordered_map<std::string, std::string> columnFilters;
        for (const auto &[columnId, filterValue] : state.Filter.ColumnFilters)
        {
            columnFilters[columnId] = pserv::utils::ToLower(filterValue);
        }

        for (const auto *obj : m_impl->Controller->GetDataObjects())
        {
            if (!obj)
            {
                continue;
            }

            if (!lowerFilter.empty() && !obj->MatchesFilter(lowerFilter))
            {
                continue;
            }

            bool matchesAllColumnFilters = true;
            for (size_t i = 0; i < m_impl->Descriptor.Columns.size(); ++i)
            {
                const auto filterIt = columnFilters.find(m_impl->Descriptor.Columns[i].Id);
                if (filterIt == columnFilters.end() || filterIt->second.empty())
                {
                    continue;
                }

                if (!pserv::utils::ContainsIgnoreCase(obj->GetProperty(static_cast<int>(i)), filterIt->second))
                {
                    matchesAllColumnFilters = false;
                    break;
                }
            }

            if (!matchesAllColumnFilters)
            {
                continue;
            }

            ExplorerItem item;
            item.Id = obj->GetStableID();
            item.State = ToVisualState(m_impl->Controller->GetVisualState(obj));
            item.Selected = std::ranges::find(state.SelectedItemIds, item.Id) != state.SelectedItemIds.end();

            for (size_t i = 0; i < m_impl->Descriptor.Columns.size(); ++i)
            {
                item.Values.push_back(ToValue(obj->GetTypedProperty(static_cast<int>(i))));
            }

            result.Items.push_back(std::move(item));
        }

        return result;
    }

    std::vector<ActionDescriptor> ServicesViewAdapter::GetActions(const ExplorerViewState &state) const
    {
        if (state.SelectedItemIds.empty())
        {
            return {};
        }

        const auto *obj = FindObject(*m_impl->Controller, state.SelectedItemIds.front());
        if (!obj)
        {
            return {};
        }

        std::vector<ActionDescriptor> result;
        for (const auto *action : m_impl->Controller->GetActions(obj))
        {
            if (!action || action->IsSeparator())
            {
                continue;
            }

            result.push_back(ActionDescriptor{
                NormalizeActionId(action->GetName()),
                action->GetName(),
                action->IsDestructive(),
                action->RequiresConfirmation(),
                true});
        }

        return result;
    }

    std::optional<DetailDocument> ServicesViewAdapter::GetDetails(std::string_view itemId) const
    {
        const auto *obj = FindObject(*m_impl->Controller, itemId);
        if (!obj)
        {
            return std::nullopt;
        }

        DetailDocument document;
        document.ItemId = std::string(itemId);
        document.Title = obj->GetItemName();

        DetailSection section;
        section.Id = "service";
        section.Title = "Service";

        for (size_t i = 0; i < m_impl->Descriptor.Columns.size(); ++i)
        {
            const auto &column = m_impl->Descriptor.Columns[i];
            DetailField field;
            field.Id = column.Id;
            field.Label = column.Title;
            field.Value = ToValue(obj->GetTypedProperty(static_cast<int>(i)));
            field.Editor = column.Editor;
            field.Editable = column.Editable;
            field.Required = (column.Id == "Name" || column.Id == "DisplayName");

            if (field.Editor == FieldEditor::Combo)
            {
                for (const auto &option : m_impl->Controller->GetComboOptions(static_cast<int>(i)))
                {
                    field.Options.push_back({pserv::utils::ToLower(option), option});
                }
            }

            section.Fields.push_back(std::move(field));
        }

        document.Sections.push_back(std::move(section));
        return document;
    }

    bool ServicesViewAdapter::ApplyDetailEdits(std::string_view itemId, const DetailDocument &document, std::string &errorMessage)
    {
        auto *obj = FindObjectMutable(*m_impl->Controller, itemId);
        if (!obj)
        {
            errorMessage = "Selected service is no longer available.";
            return false;
        }

        m_impl->Controller->BeginPropertyEdits(obj);

        const auto &columns = m_impl->Controller->GetColumns();
        for (const auto &section : document.Sections)
        {
            for (const auto &field : section.Fields)
            {
                if (!field.Editable)
                {
                    continue;
                }

                const auto it = std::ranges::find_if(
                    columns,
                    [&field](const pserv::DataObjectColumn &column)
                    {
                        return column.BindingName == field.Id;
                    });
                if (it == columns.end())
                {
                    continue;
                }

                const int columnIndex = static_cast<int>(std::distance(columns.begin(), it));
                if (!m_impl->Controller->SetPropertyEdit(obj, columnIndex, ValueToString(field.Value)))
                {
                    errorMessage = std::format("Failed to apply edit for '{}'.", field.Label);
                    return false;
                }
            }
        }

        if (!m_impl->Controller->CommitPropertyEdits(obj))
        {
            errorMessage = "Failed to commit service property edits.";
            return false;
        }

        return true;
    }

    bool ServicesViewAdapter::SetRemoteContext(const RemoteContext &context, std::string &)
    {
        m_impl->Context = context;
        m_impl->ServicesController().SetMachineName(context.TargetName);
        return true;
    }

    RemoteContext ServicesViewAdapter::GetRemoteContext() const
    {
        return m_impl->Context;
    }

    ServicesActionExecutor::ServicesActionExecutor(ServicesViewAdapter &adapter)
        : m_impl{std::make_unique<Impl>(adapter)}
    {
    }

    ServicesActionExecutor::~ServicesActionExecutor() = default;

    ActionResult ServicesActionExecutor::Execute(const ActionRequest &request, IActionExecutionHost &host)
    {
        ActionResult result;

        auto &controller = *m_impl->Adapter.m_impl->Controller;
        if (request.ItemIds.empty())
        {
            result.ErrorMessage = "No selected services.";
            return result;
        }

        auto *target = FindObjectMutable(controller, request.ItemIds.front());
        if (!target)
        {
            result.ErrorMessage = "Selected service is no longer available.";
            return result;
        }

        const pserv::DataAction *matchedAction = nullptr;
        for (const auto *action : controller.GetActions(target))
        {
            if (!action || action->IsSeparator())
            {
                continue;
            }

            if (NormalizeActionId(action->GetName()) == request.ActionId)
            {
                matchedAction = action;
                break;
            }
        }

        if (!matchedAction)
        {
            result.ErrorMessage = std::format("Action '{}' is not available for the selected service.", request.ActionId);
            return result;
        }

        if (matchedAction->RequiresConfirmation() || matchedAction->IsDestructive())
        {
            if (!host.Confirm({
                    matchedAction->GetName(),
                    std::format("Execute '{}' on {} selected service(s)?", matchedAction->GetName(), request.ItemIds.size()),
                    matchedAction->IsDestructive()}))
            {
                result.Cancelled = true;
                return result;
            }
        }

        pserv::DataActionDispatchContext ctx;
        ctx.m_pController = &controller;
        ctx.m_hWnd = nullptr;

        for (const auto &itemId : request.ItemIds)
        {
            auto *obj = FindObjectMutable(controller, itemId);
            if (!obj)
            {
                continue;
            }
            obj->Retain(REFCOUNT_DEBUG_ARGS);
            ctx.m_selectedObjects.push_back(obj);
        }

        matchedAction->Execute(ctx);

        if (ctx.m_pAsyncOp)
        {
            while (ctx.m_pAsyncOp->GetStatus() == pserv::AsyncStatus::Running)
            {
                host.ReportProgress({
                    ctx.m_pAsyncOp->GetProgress(),
                    ctx.m_pAsyncOp->GetProgressMessage()});

                if (host.IsCancellationRequested())
                {
                    ctx.m_pAsyncOp->RequestCancel();
                }

                ::Sleep(50);
            }

            host.ReportProgress({
                ctx.m_pAsyncOp->GetProgress(),
                ctx.m_pAsyncOp->GetProgressMessage()});

            if (ctx.m_pAsyncOp->GetStatus() == pserv::AsyncStatus::Failed)
            {
                result.ErrorMessage = ctx.m_pAsyncOp->GetErrorMessage();
                host.ReportError(*result.ErrorMessage);
                return result;
            }

            if (ctx.m_pAsyncOp->GetStatus() == pserv::AsyncStatus::Cancelled)
            {
                result.Cancelled = true;
                return result;
            }
        }

        result.Success = true;
        result.RequiresRefresh = ctx.m_bNeedsRefresh || ctx.m_pAsyncOp != nullptr || request.ActionId != "properties";
        return result;
    }
} // namespace pserv6::interaction
