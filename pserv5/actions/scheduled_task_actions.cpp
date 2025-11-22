#include "precomp.h"
#include <actions/scheduled_task_actions.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <models/scheduled_task_info.h>
#include <utils/string_utils.h>
#include <windows_api/scheduled_task_manager.h>

namespace pserv
{

    namespace
    {

        // Helper function to get ScheduledTaskInfo from DataObject
        inline ScheduledTaskInfo *GetTaskInfo(DataObject *obj)
        {
            return static_cast<ScheduledTaskInfo *>(obj);
        }

        inline const ScheduledTaskInfo *GetTaskInfo(const DataObject *obj)
        {
            return static_cast<const ScheduledTaskInfo *>(obj);
        }

        // ============================================================================
        // Run Action
        // ============================================================================

        class ScheduledTaskRunAction final : public DataAction
        {
        public:
            ScheduledTaskRunAction()
                : DataAction{"Run Now", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *task = GetTaskInfo(obj);
                // Can run tasks that are not already running
                return task->GetState() != ScheduledTaskState::Running;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int success = 0;
                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *task = GetTaskInfo(obj);
                    if (ScheduledTaskManager::RunTask(task))
                    {
                        success++;
                    }
                }
                spdlog::info("Started {}/{} scheduled tasks", success, ctx.m_selectedObjects.size());
            }
        };

        // ============================================================================
        // Enable/Disable Actions
        // ============================================================================

        class ScheduledTaskEnableAction final : public DataAction
        {
        public:
            ScheduledTaskEnableAction()
                : DataAction{"Enable", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *task = GetTaskInfo(obj);
                return !task->IsEnabled();
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int success = 0;
                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *task = GetTaskInfo(obj);
                    if (ScheduledTaskManager::SetTaskEnabled(task, true))
                    {
                        success++;
                    }
                }
                spdlog::info("Enabled {}/{} scheduled tasks", success, ctx.m_selectedObjects.size());
            }
        };

        class ScheduledTaskDisableAction final : public DataAction
        {
        public:
            ScheduledTaskDisableAction()
                : DataAction{"Disable", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *task = GetTaskInfo(obj);
                return task->IsEnabled();
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int success = 0;
                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *task = GetTaskInfo(obj);
                    if (ScheduledTaskManager::SetTaskEnabled(task, false))
                    {
                        success++;
                    }
                }
                spdlog::info("Disabled {}/{} scheduled tasks", success, ctx.m_selectedObjects.size());
            }
        };

        // ============================================================================
        // Delete Action
        // ============================================================================

        class ScheduledTaskDeleteAction final : public DataAction
        {
        public:
            ScheduledTaskDeleteAction()
                : DataAction{"Delete", ActionVisibility::Both}
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

            void Execute(DataActionDispatchContext &ctx) const override
            {
                std::vector<ScheduledTaskInfo *> tasks;
                std::string confirmMsg = "Are you sure you want to delete the following scheduled tasks?\n\n";

                for (auto *obj : ctx.m_selectedObjects)
                {
                    auto *task = GetTaskInfo(obj);
                    tasks.push_back(task);
                    confirmMsg += std::format("{}\n", task->GetPath());
                    if (tasks.size() >= 10)
                    {
                        confirmMsg += "... and more\n";
                        break;
                    }
                }

                if (MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Deletion", MB_YESNO | MB_ICONWARNING) == IDYES)
                {
                    int success = 0;
                    for (auto *task : tasks)
                    {
                        if (ScheduledTaskManager::DeleteTask(task))
                        {
                            success++;
                        }
                    }
                    spdlog::info("Deleted {}/{} scheduled tasks", success, tasks.size());

                    // TODO: Trigger refresh
                }
            }
        };

        // ============================================================================
        // Copy Actions
        // ============================================================================

        class ScheduledTaskCopyNameAction final : public DataAction
        {
        public:
            ScheduledTaskCopyNameAction()
                : DataAction{"Copy Name", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *task = GetTaskInfo(ctx.m_selectedObjects[0]);
                ImGui::SetClipboardText(task->GetName().c_str());
                spdlog::info("Copied task name to clipboard: {}", task->GetName());
            }
        };

        class ScheduledTaskCopyPathAction final : public DataAction
        {
        public:
            ScheduledTaskCopyPathAction()
                : DataAction{"Copy Path", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *task = GetTaskInfo(ctx.m_selectedObjects[0]);
                ImGui::SetClipboardText(task->GetPath().c_str());
                spdlog::info("Copied task path to clipboard: {}", task->GetPath());
            }
        };

        // ============================================================================
        // Edit Configuration Action (placeholder)
        // ============================================================================

        class ScheduledTaskEditAction final : public DataAction
        {
        public:
            ScheduledTaskEditAction()
                : DataAction{"Edit Configuration", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *task = GetTaskInfo(ctx.m_selectedObjects[0]);

                // Launch Task Scheduler MMC snap-in to edit task
                std::wstring command = L"control schedtasks";
                ShellExecuteW(nullptr, L"open", L"taskschd.msc", nullptr, nullptr, SW_SHOW);
                spdlog::info("Opened Task Scheduler to edit task: {}", task->GetPath());
            }
        };

        // Static action instances
        ScheduledTaskRunAction theRunAction;
        ScheduledTaskEnableAction theEnableAction;
        ScheduledTaskDisableAction theDisableAction;
        ScheduledTaskDeleteAction theDeleteAction;
        ScheduledTaskCopyNameAction theCopyNameAction;
        ScheduledTaskCopyPathAction theCopyPathAction;
        ScheduledTaskEditAction theEditAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateScheduledTaskActions(ScheduledTaskState state, bool enabled)
    {
        std::vector<const DataAction *> actions;

        // Run action
        if (state != ScheduledTaskState::Running)
        {
            actions.push_back(&theRunAction);
        }

        actions.push_back(&theDataActionSeparator);

        // Enable/Disable
        if (enabled)
        {
            actions.push_back(&theDisableAction);
        }
        else
        {
            actions.push_back(&theEnableAction);
        }

        actions.push_back(&theDataActionSeparator);

        // Edit and Delete
        actions.push_back(&theEditAction);
        actions.push_back(&theDeleteAction);

        actions.push_back(&theDataActionSeparator);

        // Copy actions
        actions.push_back(&theCopyNameAction);
        actions.push_back(&theCopyPathAction);

        return actions;
    }

} // namespace pserv
