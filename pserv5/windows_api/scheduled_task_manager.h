#pragma once
#include <core/data_object.h>
#include <models/scheduled_task_info.h>

namespace pserv
{

    class ScheduledTaskManager
    {
    public:
        // Enumerate all scheduled tasks
        static std::vector<DataObject *> EnumerateTasks();

        // Enable/disable a task
        static bool SetTaskEnabled(const ScheduledTaskInfo *task, bool enabled);

        // Run a task immediately
        static bool RunTask(const ScheduledTaskInfo *task);

        // Delete a task
        static bool DeleteTask(const ScheduledTaskInfo *task);

    private:
        // Helper to recursively enumerate tasks from a folder
        static void EnumerateTasksInFolder(ITaskFolder *pFolder,
            const std::wstring &folderPath,
            std::vector<DataObject *> &tasks);

        // Helper to extract task information
        static ScheduledTaskInfo *ExtractTaskInfo(IRegisteredTask *pTask, const std::wstring &taskPath);

        // Helper to format SYSTEMTIME to string
        static std::string FormatSystemTime(const SYSTEMTIME &st);

        // Helper to format trigger description
        static std::string FormatTriggerDescription(ITaskDefinition *pTaskDef);
    };

} // namespace pserv
