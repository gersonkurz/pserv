#pragma once


namespace pserv
{
    class DataObjectContainer;
    class ScheduledTaskInfo;

    class ScheduledTaskManager
    {
    public:
        // Enumerate all scheduled tasks
        static void EnumerateTasks(DataObjectContainer *doc);

        // Enable/disable a task
        static bool SetTaskEnabled(const ScheduledTaskInfo *task, bool enabled);

        // Run a task immediately
        static bool RunTask(const ScheduledTaskInfo *task);

        // Delete a task
        static bool DeleteTask(const ScheduledTaskInfo *task);

    private:
        // Helper to recursively enumerate tasks from a folder
        static void EnumerateTasksInFolder(ITaskFolder *pFolder,
            const std::wstring &folderPath, DataObjectContainer *doc);

        // Helper to extract task information
        static void ExtractTaskInfo(DataObjectContainer *doc, IRegisteredTask *pTask, const std::wstring &taskPath);

        // Helper to format SYSTEMTIME to string
        static std::string FormatSystemTime(const SYSTEMTIME &st);

        // Helper to format trigger description
        static std::string FormatTriggerDescription(ITaskDefinition *pTaskDef);
    };

} // namespace pserv
