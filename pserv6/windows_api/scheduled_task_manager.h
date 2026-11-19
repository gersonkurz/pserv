/// @file scheduled_task_manager.h
/// @brief Windows Task Scheduler COM API wrapper.
///
/// Provides task enumeration and control operations using the
/// Task Scheduler 2.0 COM interface.
#pragma once

namespace pserv
{
    class DataObjectContainer;
    class ScheduledTaskInfo;

    /// @brief Static class for Task Scheduler operations.
    ///
    /// Uses ITaskService COM interface to enumerate and control
    /// scheduled tasks. Recursively enumerates all task folders.
    class ScheduledTaskManager
    {
    public:
        /// @brief Enumerate all scheduled tasks into a container.
        /// @param doc Container to populate with ScheduledTaskInfo objects.
        static void EnumerateTasks(DataObjectContainer *doc);

        /// @brief Enable or disable a scheduled task.
        /// @param task The task to modify.
        /// @param enabled true to enable, false to disable.
        static bool SetTaskEnabled(const ScheduledTaskInfo *task, bool enabled);

        /// @brief Run a task immediately.
        /// @param task The task to execute.
        static bool RunTask(const ScheduledTaskInfo *task);

        /// @brief Delete a scheduled task.
        /// @param task The task to delete.
        static bool DeleteTask(const ScheduledTaskInfo *task);

    private:
        static void EnumerateTasksInFolder(ITaskFolder *pFolder,
            const std::wstring &folderPath, DataObjectContainer *doc);
        static void ExtractTaskInfo(DataObjectContainer *doc, IRegisteredTask *pTask, const std::wstring &taskPath);
        static std::string FormatSystemTime(const SYSTEMTIME &st);
        static std::string FormatTriggerDescription(ITaskDefinition *pTaskDef);
    };

} // namespace pserv
