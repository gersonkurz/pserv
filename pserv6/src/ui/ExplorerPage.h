#pragma once

#include "pch.h"

#include <unordered_map>

#include <interaction/services_adapter.h>

class ExplorerPage : public pserv6::interaction::IActionExecutionHost
{
public:
    ExplorerPage();

    winrt::Microsoft::UI::Xaml::UIElement Root() const;
    winrt::Windows::Foundation::IAsyncAction ShowPropertiesDialogAsync();

    bool Confirm(const pserv6::interaction::ConfirmationRequest &request) override;
    void ReportProgress(const pserv6::interaction::ActionProgress &progress) override;
    bool IsCancellationRequested() const override;
    void ReportError(std::string_view message) override;

private:
    using StackPanel = winrt::Microsoft::UI::Xaml::Controls::StackPanel;
    using TextBlock = winrt::Microsoft::UI::Xaml::Controls::TextBlock;
    using Grid = winrt::Microsoft::UI::Xaml::Controls::Grid;
    using Border = winrt::Microsoft::UI::Xaml::Controls::Border;
    using Button = winrt::Microsoft::UI::Xaml::Controls::Button;
    using ListView = winrt::Microsoft::UI::Xaml::Controls::ListView;

    winrt::Microsoft::UI::Xaml::UIElement m_root{nullptr};
    StackPanel m_layout{nullptr};
    StackPanel m_actionsPanel{nullptr};
    Grid m_headerGrid{nullptr};
    ListView m_listView{nullptr};
    TextBlock m_statusText{nullptr};

    pserv6::interaction::ExplorerViewState m_state;
    std::unique_ptr<pserv6::interaction::ServicesViewAdapter> m_adapter;
    std::unique_ptr<pserv6::interaction::ServicesActionExecutor> m_actionExecutor;
    std::vector<pserv6::interaction::ExplorerItem> m_items;

    void BuildShell();
    void Refresh();
    void RenderActions();
    void RenderHeader();
    void RenderList();
    void UpdateSelectionFromList();
    void ExecuteAction(const std::string &actionId);
    static double GetColumnWidth(const pserv6::interaction::ColumnDescriptor &column, size_t index);
};
