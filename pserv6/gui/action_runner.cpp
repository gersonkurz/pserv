#include "pch.h"
#include "action_runner.h"
#include "properties_dialog.h"
#include <core/async_operation.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <utils/file_dialogs.h>
#include <utils/string_utils.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace pserv6
{
    // --- RetainedSelection -------------------------------------------------------------------

    RetainedSelection::RetainedSelection(const std::vector<pserv::DataObject *> &objects)
        : m_objects{objects}
    {
        for (auto *p : m_objects)
            p->Retain(REFCOUNT_DEBUG_ARGS);
    }

    RetainedSelection::~RetainedSelection()
    {
        for (auto *p : m_objects)
            p->Release(REFCOUNT_DEBUG_ARGS);
    }

    RetainedSelection &RetainedSelection::operator=(RetainedSelection &&other) noexcept
    {
        if (this != &other)
        {
            for (auto *p : m_objects)
                p->Release(REFCOUNT_DEBUG_ARGS);
            m_objects = std::exchange(other.m_objects, {});
        }
        return *this;
    }

    // --- ActionRunner ------------------------------------------------------------------------

    ActionRunner::ActionRunner(Callbacks callbacks)
        : m_callbacks{std::move(callbacks)}
    {
        m_dispatch.m_pHost = this;
        m_dispatch.m_hWnd = m_callbacks.hwnd;
    }

    ActionRunner::~ActionRunner()
    {
        Shutdown();
        // m_dispatch's destructor cancels and waits for a still running operation.
    }

    void ActionRunner::Shutdown()
    {
        *m_alive = false;
        if (m_progressTimer)
            m_progressTimer.Stop();
    }

    void ActionRunner::SetDialogOpen(bool bOpen)
    {
        const bool bWasPaused = IsBusy();
        m_bDialogOpen = bOpen;
        if (IsBusy() != bWasPaused && m_callbacks.setRefreshPaused)
            m_callbacks.setRefreshPaused(IsBusy());
    }

    ContentDialog ActionRunner::NewDialog(const std::string &title)
    {
        ContentDialog dialog;
        const auto xamlRoot = m_callbacks.xamlRoot();
        dialog.XamlRoot(xamlRoot);
        // A dialog lives in the popup layer and does not inherit the window content's theme
        if (auto content = xamlRoot.Content().try_as<FrameworkElement>())
            dialog.RequestedTheme(content.ActualTheme());
        dialog.Title(box_value(to_hstring(title)));
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Style>());
        return dialog;
    }

    bool ActionRunner::ReserveDialog()
    {
        if (IsBusy())
            return false;
        SetDialogOpen(true);
        return true;
    }

    void ActionRunner::ReleaseDialog()
    {
        SetDialogOpen(false);
        ShowNextMessage();
    }

    bool ActionRunner::RefuseIfTargetChanged(uint64_t generation, const std::string &what)
    {
        if (generation == CurrentGeneration())
            return false;
        ShowMessage(pserv::ActionMessageKind::Information, what,
            "The machine changed after this was opened; nothing was done. Please select the rows again.");
        return true;
    }

    void ActionRunner::RequestAction(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
        std::optional<uint64_t> generation)
    {
        if (!pAction || !pController || pAction->IsSeparator())
            return;
        const uint64_t requestGeneration = generation.value_or(CurrentGeneration());
        if (RefuseIfTargetChanged(requestGeneration, pAction->GetName()))
            return;
        if (pAction->RequiresLocalMachine() && m_callbacks.isRemote && m_callbacks.isRemote(pController))
        {
            ShowMessage(pserv::ActionMessageKind::Information, pAction->GetName(), "This action is only available for objects of this machine.");
            return;
        }
        if (IsBusy())
        {
            if (m_callbacks.status)
                m_callbacks.status("Another action is still running");
            return;
        }
        const bool bWasPaused = IsBusy();
        m_bBusy = true;
        if (!bWasPaused && m_callbacks.setRefreshPaused)
            m_callbacks.setRefreshPaused(true);
        RunRequest(pAction, pController, RetainedSelection{selection}, requestGeneration);
    }

    winrt::fire_and_forget ActionRunner::RunRequest(const pserv::DataAction *pAction, pserv::DataController *pController, RetainedSelection selection, uint64_t generation)
    {
        std::weak_ptr<bool> alive = m_alive;
        auto finishCancelled = [this]
        {
            const bool bWasPaused = IsBusy();
            m_bBusy = false;
            if (bWasPaused && !IsBusy() && m_callbacks.setRefreshPaused)
                m_callbacks.setRefreshPaused(false);
            ShowNextMessage();
        };

        // 1. Confirmation (Execute() never asks)
        if (pAction->RequiresConfirmation())
        {
            auto dialog = NewDialog(std::format("Confirm: {}", pAction->GetName()));
            TextBlock text;
            text.Text(to_hstring(pAction->GetConfirmationText(selection.Get())));
            text.TextWrapping(TextWrapping::Wrap);
            dialog.Content(text);
            dialog.PrimaryButtonText(L"Yes");
            dialog.CloseButtonText(L"No");
            // A destructive action is not confirmed by a stray Enter
            dialog.DefaultButton(pAction->IsDestructive() ? ContentDialogButton::Close : ContentDialogButton::Primary);
            SetDialogOpen(true);
            const auto result = co_await dialog.ShowAsync();
            if (auto a = alive.lock(); !a || !*a)
                co_return;
            SetDialogOpen(false);
            if (result != ContentDialogResult::Primary)
            {
                finishCancelled();
                co_return;
            }
        }

        // 2. Parameters: file paths through the native save dialog, text through a dialog
        std::map<std::string, std::string> values;
        std::vector<pserv::ActionParameter> textParameters;
        for (const auto &parameter : pAction->GetParameters(pController))
        {
            if (parameter.Kind == pserv::ActionParameterKind::SaveFilePath)
            {
                std::vector<pserv::utils::FileTypeFilter> filters;
                for (const auto &f : parameter.FileFilters)
                    filters.push_back({pserv::utils::Utf8ToWide(f.Name), pserv::utils::Utf8ToWide(f.Pattern)});
                std::wstring path;
                if (!pserv::utils::SaveFileDialog(m_callbacks.hwnd, pserv::utils::Utf8ToWide(parameter.Label), pserv::utils::Utf8ToWide(parameter.DefaultValue), filters, 0, path))
                {
                    finishCancelled();
                    co_return;
                }
                values[parameter.Name] = pserv::utils::WideToUtf8(path);
            }
            else
            {
                textParameters.push_back(parameter);
            }
        }
        if (!textParameters.empty())
        {
            auto dialog = NewDialog(pAction->GetName());
            StackPanel panel;
            panel.Spacing(8);
            std::vector<TextBox> boxes;
            for (const auto &parameter : textParameters)
            {
                TextBox box;
                box.Header(box_value(to_hstring(parameter.Label)));
                box.Text(to_hstring(parameter.DefaultValue));
                box.MinWidth(380);
                panel.Children().Append(box);
                boxes.push_back(box);
            }
            dialog.Content(panel);
            dialog.PrimaryButtonText(L"OK");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
            SetDialogOpen(true);
            const auto result = co_await dialog.ShowAsync();
            if (auto a = alive.lock(); !a || !*a)
                co_return;
            SetDialogOpen(false);
            if (result != ContentDialogResult::Primary)
            {
                finishCancelled();
                co_return;
            }
            for (size_t i = 0; i < textParameters.size(); ++i)
                values[textParameters[i].Name] = to_string(boxes[i].Text());
        }

        // 3. Execute, unless the target changed while the dialogs were open
        if (RefuseIfTargetChanged(generation, pAction->GetName()))
        {
            finishCancelled();
            co_return;
        }
        Execute(pAction, pController, selection.Get(), std::move(values));
    }

    void ActionRunner::ExecuteNow(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
        std::map<std::string, std::string> parameters)
    {
        if (IsBusy() || !pAction || !pController)
            return;
        m_bBusy = true;
        if (m_callbacks.setRefreshPaused)
            m_callbacks.setRefreshPaused(true);
        Execute(pAction, pController, selection, std::move(parameters));
    }

    void ActionRunner::Execute(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
        std::map<std::string, std::string> parameters)
    {
        spdlog::info("Action '{}' on {} object(s)", pAction->GetName(), selection.size());
        // Section 5.1: an enumeration that started before the action must not publish over its
        // effect (its result is discarded as stale); a refresh follows the action.
        pController->InvalidatePendingRefreshes();
        m_dispatch.SetSelection(selection);
        m_dispatch.m_pController = pController;
        m_dispatch.m_hWnd = m_callbacks.hwnd;
        m_dispatch.m_pHost = this;
        m_dispatch.m_parameters = std::move(parameters);
        m_dispatch.m_bNeedsRefresh = false;
        m_dispatch.m_bShowProgressDialog = false;
        m_actionName = pAction->GetName();
        try
        {
            pAction->Execute(m_dispatch);
        }
        catch (const std::exception &e)
        {
            spdlog::error("Action '{}' failed: {}", pAction->GetName(), e.what());
            ShowMessage(pserv::ActionMessageKind::Error, pAction->GetName(), e.what());
        }

        if (m_dispatch.m_pAsyncOp)
        {
            // Runs on; FinishAsync() completes it (WM_ASYNC_OPERATION_COMPLETE, or the progress
            // timer noticing the end of the operation).
            m_bAsyncFinished = false;
            ShowProgress();
            return;
        }

        // Refresh after every action: not every mutating action sets m_bNeedsRefresh (e.g. the
        // startup entries' Enable/Disable), and a copy's refresh costs one enumeration.
        m_dispatch.SetSelection({});
        m_bBusy = false;
        if (!IsBusy() && m_callbacks.setRefreshPaused)
            m_callbacks.setRefreshPaused(false);
        if (m_callbacks.refresh)
            m_callbacks.refresh(pController); // invalidates again, then requests
        if (m_callbacks.status)
            m_callbacks.status(std::format("{}: done", m_actionName));
        ShowNextMessage();
    }

    void ActionRunner::ShowProgress()
    {
        // Poll the operation for its progress text and end (100 ms); the end also arrives as
        // WM_ASYNC_OPERATION_COMPLETE, whichever is first.
        if (!m_progressTimer)
        {
            m_progressTimer = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
            m_progressTimer.Interval(std::chrono::milliseconds(100));
            m_progressTimer.IsRepeating(true);
            m_progressTimer.Tick([this, alive = std::weak_ptr<bool>(m_alive)](auto &&, auto &&)
                {
                    if (auto a = alive.lock(); !a || !*a)
                        return;
                    auto *op = m_dispatch.m_pAsyncOp;
                    if (!op)
                        return;
                    if (m_progressBar)
                    {
                        m_progressBar.Value(op->GetProgress());
                        m_progressText.Text(to_hstring(op->GetProgressMessage()));
                    }
                    if (op->GetStatus() != pserv::AsyncStatus::Running)
                        FinishAsync();
                });
        }
        m_progressTimer.Start();

        if (!m_dispatch.m_bShowProgressDialog || m_bDialogOpen || m_bAutomation)
            return;
        m_progressDialog = NewDialog(m_actionName);
        StackPanel panel;
        panel.Spacing(10);
        panel.MinWidth(400);
        m_progressText = TextBlock{};
        m_progressText.TextWrapping(TextWrapping::Wrap);
        m_progressText.Text(L"Starting...");
        m_progressBar = ProgressBar{};
        m_progressBar.Minimum(0);
        m_progressBar.Maximum(1);
        panel.Children().Append(m_progressText);
        panel.Children().Append(m_progressBar);
        m_progressDialog.Content(panel);
        m_progressDialog.CloseButtonText(L"Cancel");
        m_progressDialog.CloseButtonClick([this](auto &&, ContentDialogButtonClickEventArgs const &args)
            {
                // Keep the dialog until the operation has actually stopped
                args.Cancel(true);
                if (auto *op = m_dispatch.m_pAsyncOp)
                {
                    op->RequestCancel();
                    if (m_progressText)
                        m_progressText.Text(L"Cancelling...");
                }
            });
        SetDialogOpen(true);
        [](ActionRunner *self, ContentDialog dialog, std::weak_ptr<bool> alive) -> winrt::fire_and_forget
        {
            co_await dialog.ShowAsync();
            if (auto a = alive.lock(); !a || !*a)
                co_return;
            self->SetDialogOpen(false);
            self->ShowNextMessage();
        }(this, m_progressDialog, m_alive);
    }

    void ActionRunner::OnAsyncComplete()
    {
        FinishAsync();
    }

    void ActionRunner::FinishAsync()
    {
        auto *op = m_dispatch.m_pAsyncOp;
        if (m_bAsyncFinished || !op || op->GetStatus() == pserv::AsyncStatus::Running)
            return;
        m_bAsyncFinished = true;
        if (m_progressTimer)
            m_progressTimer.Stop();

        const auto status = op->GetStatus();
        const std::string error = op->GetErrorMessage();
        const std::string message = op->GetProgressMessage();
        op->Wait(); // the worker may still be posting its completion message
        delete op;
        m_dispatch.m_pAsyncOp = nullptr;
        pserv::DataController *pController = m_dispatch.m_pController;
        m_dispatch.SetSelection({});

        if (m_progressDialog)
        {
            m_progressDialog.Hide(); // its ShowAsync continuation frees the dialog slot
            m_progressDialog = nullptr;
            m_progressBar = nullptr;
            m_progressText = nullptr;
        }

        switch (status)
        {
        case pserv::AsyncStatus::Completed:
            spdlog::info("Action '{}' completed: {}", m_actionName, message);
            if (m_callbacks.status)
                m_callbacks.status(message.empty() ? std::format("{}: done", m_actionName) : message);
            break;
        case pserv::AsyncStatus::Cancelled:
            spdlog::info("Action '{}' cancelled", m_actionName);
            if (m_callbacks.status)
                m_callbacks.status(std::format("{}: cancelled", m_actionName));
            break;
        default:
            spdlog::error("Action '{}' failed: {}", m_actionName, error);
            ShowMessage(pserv::ActionMessageKind::Error, m_actionName, error.empty() ? "The operation failed (see the log)." : error);
            break;
        }

        m_bBusy = false;
        if (!IsBusy() && m_callbacks.setRefreshPaused)
            m_callbacks.setRefreshPaused(false);
        // The system changed (or may have, partially): show its actual state
        if (m_callbacks.refresh && pController)
            m_callbacks.refresh(pController);
        ShowNextMessage();
    }

    void ActionRunner::ShowMessage(pserv::ActionMessageKind kind, std::string_view title, std::string_view text)
    {
        const char *prefix = kind == pserv::ActionMessageKind::Error ? "Error: " : (kind == pserv::ActionMessageKind::Warning ? "Warning: " : "");
        if (m_bAutomation)
        {
            m_recorded.push_back(std::format("{}{}: {}", prefix, title, text));
            return;
        }
        m_messages.emplace_back(std::format("{}{}", prefix, title), std::string{text});
        ShowNextMessage();
    }

    winrt::fire_and_forget ActionRunner::ShowNextMessage()
    {
        if (m_bDialogOpen || m_messages.empty() || !*m_alive)
            co_return;
        auto [title, text] = std::move(m_messages.front());
        m_messages.pop_front();
        std::weak_ptr<bool> alive = m_alive;
        auto dialog = NewDialog(title);
        TextBlock block;
        block.Text(to_hstring(text));
        block.TextWrapping(TextWrapping::Wrap);
        block.IsTextSelectionEnabled(true);
        dialog.Content(block);
        dialog.CloseButtonText(L"OK");
        dialog.DefaultButton(ContentDialogButton::Close);
        SetDialogOpen(true);
        co_await dialog.ShowAsync();
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        SetDialogOpen(false);
        ShowNextMessage();
    }

    void ActionRunner::ShowProperties(pserv::DataActionDispatchContext &ctx)
    {
        if (ctx.m_selectedObjects.empty() || !ctx.m_pController)
            return;
        if (m_bAutomation)
        {
            m_recorded.push_back(std::format("properties requested for {} object(s)", ctx.m_selectedObjects.size()));
            return;
        }
        RunProperties(ctx.m_pController, RetainedSelection{ctx.m_selectedObjects});
    }

    winrt::fire_and_forget ActionRunner::RunProperties(pserv::DataController *pController, RetainedSelection selection)
    {
        // Called from Execute() of the properties action: let it finish first. The lifetime
        // token is taken before the suspension: afterwards `this` may be gone.
        std::weak_ptr<bool> alive = m_alive;
        co_await wil::resume_foreground(winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
            winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low);
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        if (m_bDialogOpen)
            co_return; // another dialog took the slot meanwhile
        const bool bRemote = m_callbacks.isRemote && m_callbacks.isRemote(pController);
        const uint64_t generation = CurrentGeneration();
        auto properties = std::make_shared<PropertiesDialog>(pController, selection.Get(), bRemote);
        auto dialog = NewDialog(properties->Title());
        properties->Build(dialog);
        SetDialogOpen(true);
        const auto result = co_await dialog.ShowAsync();
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        SetDialogOpen(false);

        // The machine changed while the dialog was open: its objects are not what they were
        if ((result == ContentDialogResult::Primary || properties->RequestedAction()) && RefuseIfTargetChanged(generation, "Properties"))
            co_return;
        if (result == ContentDialogResult::Primary)
        {
            const auto outcome = properties->ApplyEdits();
            for (const auto &error : outcome.errors)
                ShowMessage(pserv::ActionMessageKind::Error, "Properties", error);
            if (outcome.bChanged)
            {
                // An older enumeration must not overwrite the committed edit
                pController->InvalidatePendingRefreshes();
                if (m_callbacks.refresh)
                    m_callbacks.refresh(pController);
            }
        }
        if (const auto *pAction = properties->RequestedAction(); pAction && properties->CurrentObject())
        {
            RequestAction(pAction, pController, {properties->CurrentObject()}, generation);
            co_return;
        }
        ShowNextMessage();
    }
} // namespace pserv6
