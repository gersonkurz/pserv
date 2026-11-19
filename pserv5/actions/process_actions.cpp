#include "precomp.h"
#include <core/async_operation.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <models/process_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/process_manager.h>

namespace pserv
{

    // Forward declare to avoid circular dependency
    class ProcessesDataController;

    namespace
    {

        // Helper function to get ProcessInfo from DataObject
        inline const ProcessInfo *GetProcessInfo(const DataObject *obj)
        {
            return static_cast<const ProcessInfo *>(obj);
        }

        // ============================================================================
        // File System Actions
        // ============================================================================

        class ProcessOpenLocationAction final : public DataAction
        {
        public:
            ProcessOpenLocationAction()
                : DataAction{"Open File Location", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return !GetProcessInfo(obj)->GetPath().empty();
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    const auto *proc = GetProcessInfo(obj);
                    std::string path = proc->GetPath();
                    if (!path.empty())
                    {
                        // Select file in explorer
                        std::wstring wPath = utils::Utf8ToWide(path);
                        std::wstring cmd = L"/select,\"" + wPath + L"\"";
                        HINSTANCE result = ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL, SW_SHOW);
                        if (reinterpret_cast<INT_PTR>(result) <= 32)
                        {
                            LogWin32Error("ShellExecuteW", "path '{}'", path);
                        }
                    }
                }
            }
        };

        // ============================================================================
        // Priority Actions
        // ============================================================================

        class ProcessSetPriorityAction final : public DataAction
        {
        private:
            DWORD m_priorityClass;

        public:
            ProcessSetPriorityAction(std::string name, DWORD priorityClass)
                : DataAction{std::move(name), ActionVisibility::ContextMenu},
                  m_priorityClass{priorityClass}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<DWORD> pids;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    pids.push_back(GetProcessInfo(obj)->GetPid());
                }

                // No async needed for priority usually, it's fast
                int success = 0;
                for (DWORD pid : pids)
                {
                    if (ProcessManager::SetProcessPriority(pid, m_priorityClass))
                    {
                        success++;
                    }
                }
                spdlog::info("Set priority for {}/{} processes", success, pids.size());

                // Signal refresh needed
                // Note: This will be handled by the controller after we integrate
            }
        };

        // ============================================================================
        // Terminate Action
        // ============================================================================

        class ProcessTerminateAction final : public DataAction
        {
        public:
            ProcessTerminateAction()
                : DataAction{"Terminate Process", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            bool IsDestructive() const override
            {
                return true;
            }

            bool RequiresConfirmation() const override
            {
                return true;
            }

            std::string GetConfirmationText(const std::vector<DataObject *> &selection) const override
            {
                std::vector<std::string> items;
                for (const auto *obj : selection)
                {
                    const auto *proc = GetProcessInfo(obj);
                    items.push_back(std::format("{} (PID: {})", proc->GetName(), proc->GetPid()));
                }
                return FormatConfirmationList("Are you sure you want to terminate the following processes?", items);
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<DWORD> pids;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    pids.push_back(GetProcessInfo(obj)->GetPid());
                }

                // Same pattern as the service actions, in every frontend: the GUI shows progress,
                // the console waits for ctx.m_pAsyncOp after Execute() returns.
                if (ctx.m_pAsyncOp)
                {
                    ctx.m_pAsyncOp->Wait();
                    delete ctx.m_pAsyncOp;
                    ctx.m_pAsyncOp = nullptr;
                }

                ctx.m_pAsyncOp = DBG_NEW AsyncOperation();
                ctx.m_bShowProgressDialog = true;

                ctx.m_pAsyncOp->Start(ctx.m_hWnd,
                    [pids](AsyncOperation *op) -> bool
                    {
                        size_t total = pids.size();
                        size_t success = 0;
                        for (size_t i = 0; i < total; ++i)
                        {
                            op->ReportProgress(static_cast<float>(i) / total, std::format("Terminating process PID {}...", pids[i]));
                            if (ProcessManager::TerminateProcessById(pids[i]))
                            {
                                success++;
                            }
                        }
                        op->ReportProgress(1.0f, std::format("Terminated {}/{} processes", success, total));
                        spdlog::info("Terminated {}/{} processes", success, total);
                        // Any failure fails the operation; the message above becomes its error text.
                        return success == total;
                    });
            }
        };

        ProcessOpenLocationAction theProcessOpenLocationAction;
        ProcessSetPriorityAction theSetRealtimePriorityAction{"Set Priority: Realtime", REALTIME_PRIORITY_CLASS};
        ProcessSetPriorityAction theSetHighPriorityAction{"Set Priority: High", HIGH_PRIORITY_CLASS};
        ProcessSetPriorityAction theSetAboveNormalPriorityAction{"Set Priority: Above Normal", ABOVE_NORMAL_PRIORITY_CLASS};
        ProcessSetPriorityAction theSetNormalPriorityAction{"Set Priority: Normal", NORMAL_PRIORITY_CLASS};
        ProcessSetPriorityAction theSetBelowNormalPriorityAction{"Set Priority: Below Normal", BELOW_NORMAL_PRIORITY_CLASS};
        ProcessSetPriorityAction theSetLowPriorityAction{"Set Priority: Low", IDLE_PRIORITY_CLASS};
        ProcessTerminateAction theProcessTerminateAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateProcessActions()
    {
        return {&theProcessOpenLocationAction,
            &theDataActionSeparator,
            &theSetRealtimePriorityAction,
            &theSetHighPriorityAction,
            &theSetAboveNormalPriorityAction,
            &theSetNormalPriorityAction,
            &theSetBelowNormalPriorityAction,
            &theSetLowPriorityAction,
            &theDataActionSeparator,
            &theProcessTerminateAction};
    }

    std::vector<const DataAction *> CreateAllProcessActions()
    {
        // Console: Return all actions regardless of process state
        return {
            &theProcessOpenLocationAction,
            &theDataActionSeparator,
            &theSetRealtimePriorityAction,
            &theSetHighPriorityAction,
            &theSetAboveNormalPriorityAction,
            &theSetNormalPriorityAction,
            &theSetBelowNormalPriorityAction,
            &theSetLowPriorityAction,
            &theDataActionSeparator,
            &theProcessTerminateAction};
    }

} // namespace pserv
