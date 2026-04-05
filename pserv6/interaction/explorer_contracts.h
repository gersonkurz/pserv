#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pserv6::interaction
{
    using Value = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string>;

    enum class ColumnKind
    {
        Text,
        Integer,
        UnsignedInteger,
        Size,
        Time,
        Boolean
    };

    enum class FieldEditor
    {
        ReadOnly,
        Text,
        MultilineText,
        Combo
    };

    enum class SelectionMode
    {
        None,
        Single,
        Multiple
    };

    enum class VisualState
    {
        Normal,
        Highlighted,
        Disabled,
        Warning,
        Error
    };

    struct ColumnDescriptor
    {
        std::string Id;
        std::string Title;
        ColumnKind Kind{ColumnKind::Text};
        bool Sortable{true};
        bool Filterable{true};
        bool VisibleByDefault{true};
        bool Editable{false};
        FieldEditor Editor{FieldEditor::ReadOnly};
    };

    struct ExplorerItem
    {
        std::string Id;
        std::vector<Value> Values;
        VisualState State{VisualState::Normal};
        bool Selected{false};
    };

    struct FilterState
    {
        std::string GlobalText;
        std::vector<std::pair<std::string, std::string>> ColumnFilters;
    };

    struct SortState
    {
        std::string ColumnId;
        bool Ascending{true};
    };

    struct FieldOption
    {
        std::string Id;
        std::string Label;
    };

    struct DetailField
    {
        std::string Id;
        std::string Label;
        Value Value;
        FieldEditor Editor{FieldEditor::ReadOnly};
        bool Editable{false};
        bool Required{false};
        std::vector<FieldOption> Options;
    };

    struct DetailSection
    {
        std::string Id;
        std::string Title;
        std::vector<DetailField> Fields;
    };

    struct DetailDocument
    {
        std::string ItemId;
        std::string Title;
        std::vector<DetailSection> Sections;
        bool HasPendingEdits{false};
    };

    struct ActionDescriptor
    {
        std::string Id;
        std::string Label;
        bool Destructive{false};
        bool RequiresConfirmation{false};
        bool Enabled{true};
    };

    struct ExplorerViewState
    {
        FilterState Filter;
        std::optional<SortState> Sort;
        std::vector<std::string> SelectedItemIds;
        std::optional<std::string> FocusedItemId;
    };

    struct ExplorerViewDescriptor
    {
        std::string Id;
        std::string Title;
        std::string ItemNameSingular;
        std::string ItemNamePlural;
        SelectionMode Selection{SelectionMode::Multiple};
        bool SupportsAutoRefresh{true};
        bool SupportsDetails{true};
        bool SupportsRemoteContext{false};
        std::vector<ColumnDescriptor> Columns;
    };

    struct ViewRefreshResult
    {
        std::vector<ExplorerItem> Items;
        std::optional<std::string> ErrorMessage;
    };

    struct RemoteContext
    {
        std::string TargetName;
        bool IsRemote() const { return !TargetName.empty(); }
    };

    class IExplorerViewAdapter
    {
    public:
        virtual ~IExplorerViewAdapter() = default;

        virtual const ExplorerViewDescriptor &Describe() const = 0;
        virtual ViewRefreshResult Refresh(const ExplorerViewState &state, bool isAutoRefresh) = 0;
        virtual std::vector<ActionDescriptor> GetActions(const ExplorerViewState &state) const = 0;
        virtual std::optional<DetailDocument> GetDetails(std::string_view itemId) const = 0;
        virtual bool ApplyDetailEdits(std::string_view itemId, const DetailDocument &document, std::string &errorMessage) = 0;
        virtual bool SetRemoteContext(const RemoteContext &context, std::string &errorMessage) = 0;
        virtual RemoteContext GetRemoteContext() const = 0;
    };
} // namespace pserv6::interaction

