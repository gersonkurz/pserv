#include "precomp.h"
#include <comdef.h>
#include <taskschd.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/scheduled_task_manager.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

namespace pserv
{

    std::vector<DataObject *> ScheduledTaskManager::EnumerateTasks()
    {
        std::vector<DataObject *> tasks;

        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool bComInitialized = SUCCEEDED(hr);

        if (!bComInitialized && hr != RPC_E_CHANGED_MODE)
        {
            LogWin32ErrorCode("CoInitializeEx", hr, "initializing COM for Task Scheduler");
            return tasks;
        }
        else
        {
            // Create Task Service instance
            wil::com_ptr<ITaskService> pService;
            hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void **)&pService);

            if (FAILED(hr))
            {
                LogWin32ErrorCode("CoCreateInstance(TaskScheduler)", hr, "creating task service");
                if (bComInitialized)
                    CoUninitialize();
                return tasks;
            }

            // Connect to the Task Scheduler service
            hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
            if (FAILED(hr))
            {
                LogWin32ErrorCode("ITaskService::Connect", hr, "connecting to task scheduler");
                if (bComInitialized)
                    CoUninitialize();
                return tasks;
            }

            // Get the root folder
            wil::com_ptr<ITaskFolder> pRootFolder;
            hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
            if (FAILED(hr))
            {
                LogWin32ErrorCode("ITaskService::GetFolder", hr, "getting root folder");
                if (bComInitialized)
                    CoUninitialize();
                return tasks;
            }

            // Recursively enumerate all tasks
            EnumerateTasksInFolder(pRootFolder.get(), L"\\", tasks);
        }

        if (bComInitialized)
            CoUninitialize();

        return tasks;
    }

    void ScheduledTaskManager::EnumerateTasksInFolder(ITaskFolder *pFolder, const std::wstring &folderPath, std::vector<DataObject *> &tasks)
    {
        if (!pFolder)
            return;

        // Enumerate tasks in current folder
        wil::com_ptr<IRegisteredTaskCollection> pTaskCollection;
        HRESULT hr = pFolder->GetTasks(TASK_ENUM_HIDDEN, &pTaskCollection);
        if (SUCCEEDED(hr) && pTaskCollection)
        {
            LONG taskCount = 0;
            hr = pTaskCollection->get_Count(&taskCount);
            if (SUCCEEDED(hr))
            {
                for (LONG i = 1; i <= taskCount; i++) // COM collections are 1-based
                {
                    wil::com_ptr<IRegisteredTask> pTask;
                    hr = pTaskCollection->get_Item(_variant_t(i), &pTask);
                    if (SUCCEEDED(hr) && pTask)
                    {
                        BSTR taskName = nullptr;
                        hr = pTask->get_Name(&taskName);
                        if (SUCCEEDED(hr) && taskName)
                        {
                            std::wstring taskPath = folderPath;
                            if (taskPath != L"\\")
                                taskPath += L"\\";
                            taskPath += taskName;

                            ScheduledTaskInfo *taskInfo = ExtractTaskInfo(pTask.get(), taskPath);
                            if (taskInfo)
                            {
                                tasks.push_back(taskInfo);
                            }

                            SysFreeString(taskName);
                        }
                    }
                }
            }
        }

        // Recursively enumerate subfolders
        wil::com_ptr<ITaskFolderCollection> pFolderCollection;
        hr = pFolder->GetFolders(0, &pFolderCollection);
        if (SUCCEEDED(hr) && pFolderCollection)
        {
            LONG folderCount = 0;
            hr = pFolderCollection->get_Count(&folderCount);
            if (SUCCEEDED(hr))
            {
                for (LONG i = 1; i <= folderCount; i++)
                {
                    wil::com_ptr<ITaskFolder> pSubFolder;
                    hr = pFolderCollection->get_Item(_variant_t(i), &pSubFolder);
                    if (SUCCEEDED(hr) && pSubFolder)
                    {
                        BSTR subFolderName = nullptr;
                        hr = pSubFolder->get_Name(&subFolderName);
                        if (SUCCEEDED(hr) && subFolderName)
                        {
                            std::wstring subFolderPath = folderPath;
                            if (subFolderPath != L"\\")
                                subFolderPath += L"\\";
                            subFolderPath += subFolderName;

                            EnumerateTasksInFolder(pSubFolder.get(), subFolderPath, tasks);

                            SysFreeString(subFolderName);
                        }
                    }
                }
            }
        }
    }

    ScheduledTaskInfo *ScheduledTaskManager::ExtractTaskInfo(IRegisteredTask *pTask, const std::wstring &taskPath)
    {
        if (!pTask)
            return nullptr;

        std::string name = utils::WideToUtf8(taskPath);
        std::string path = name;
        std::string statusString = "Unknown";
        std::string trigger = "";
        std::string lastRunTime = "Never";
        std::string nextRunTime = "N/A";
        std::string author = "";
        bool enabled = false;
        ScheduledTaskState state = ScheduledTaskState::Unknown;

        // Get state
        TASK_STATE taskState;
        HRESULT hr = pTask->get_State(&taskState);
        if (SUCCEEDED(hr))
        {
            switch (taskState)
            {
            case TASK_STATE_UNKNOWN:
                statusString = "Unknown";
                state = ScheduledTaskState::Unknown;
                break;
            case TASK_STATE_DISABLED:
                statusString = "Disabled";
                state = ScheduledTaskState::Disabled;
                break;
            case TASK_STATE_QUEUED:
                statusString = "Queued";
                state = ScheduledTaskState::Queued;
                break;
            case TASK_STATE_READY:
                statusString = "Ready";
                state = ScheduledTaskState::Ready;
                break;
            case TASK_STATE_RUNNING:
                statusString = "Running";
                state = ScheduledTaskState::Running;
                break;
            }
        }

        // Get enabled status
        VARIANT_BOOL vbEnabled = VARIANT_FALSE;
        hr = pTask->get_Enabled(&vbEnabled);
        if (SUCCEEDED(hr))
        {
            enabled = (vbEnabled == VARIANT_TRUE);
        }

        // Get last run time
        DATE lastRun;
        hr = pTask->get_LastRunTime(&lastRun);
        if (SUCCEEDED(hr) && lastRun != 0)
        {
            SYSTEMTIME st;
            if (VariantTimeToSystemTime(lastRun, &st))
            {
                lastRunTime = FormatSystemTime(st);
            }
        }

        // Get next run time
        DATE nextRun;
        hr = pTask->get_NextRunTime(&nextRun);
        if (SUCCEEDED(hr) && nextRun != 0)
        {
            SYSTEMTIME st;
            if (VariantTimeToSystemTime(nextRun, &st))
            {
                nextRunTime = FormatSystemTime(st);
            }
        }

        // Get task definition for additional info
        wil::com_ptr<ITaskDefinition> pTaskDef;
        hr = pTask->get_Definition(&pTaskDef);
        if (SUCCEEDED(hr) && pTaskDef)
        {
            // Get author/registration info
            wil::com_ptr<IRegistrationInfo> pRegInfo;
            hr = pTaskDef->get_RegistrationInfo(&pRegInfo);
            if (SUCCEEDED(hr) && pRegInfo)
            {
                BSTR authorBstr = nullptr;
                hr = pRegInfo->get_Author(&authorBstr);
                if (SUCCEEDED(hr) && authorBstr)
                {
                    author = utils::WideToUtf8(authorBstr);
                    SysFreeString(authorBstr);
                }
            }

            // Get trigger description
            trigger = FormatTriggerDescription(pTaskDef.get());
        }

        // Extract just the task name (last component of path)
        size_t lastSlash = name.find_last_of('\\');
        if (lastSlash != std::string::npos)
        {
            name = name.substr(lastSlash + 1);
        }

        return new ScheduledTaskInfo(name, path, statusString, trigger, lastRunTime, nextRunTime, author, enabled, state);
    }

    std::string ScheduledTaskManager::FormatSystemTime(const SYSTEMTIME &st)
    {
        if (st.wYear == 0)
            return "N/A";

        return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }

    std::string ScheduledTaskManager::FormatTriggerDescription(ITaskDefinition *pTaskDef)
    {
        if (!pTaskDef)
            return "";

        wil::com_ptr<ITriggerCollection> pTriggerCollection;
        HRESULT hr = pTaskDef->get_Triggers(&pTriggerCollection);
        if (FAILED(hr) || !pTriggerCollection)
            return "";

        LONG triggerCount = 0;
        hr = pTriggerCollection->get_Count(&triggerCount);
        if (FAILED(hr) || triggerCount == 0)
            return "No triggers";

        if (triggerCount == 1)
        {
            wil::com_ptr<ITrigger> pTrigger;
            hr = pTriggerCollection->get_Item(1, &pTrigger);
            if (SUCCEEDED(hr) && pTrigger)
            {
                TASK_TRIGGER_TYPE2 triggerType;
                hr = pTrigger->get_Type(&triggerType);
                if (SUCCEEDED(hr))
                {
                    switch (triggerType)
                    {
                    case TASK_TRIGGER_EVENT:
                        return "On an event";
                    case TASK_TRIGGER_TIME:
                        return "At a specific time";
                    case TASK_TRIGGER_DAILY:
                        return "Daily";
                    case TASK_TRIGGER_WEEKLY:
                        return "Weekly";
                    case TASK_TRIGGER_MONTHLY:
                        return "Monthly";
                    case TASK_TRIGGER_MONTHLYDOW:
                        return "Monthly (day of week)";
                    case TASK_TRIGGER_IDLE:
                        return "On idle";
                    case TASK_TRIGGER_REGISTRATION:
                        return "At task registration";
                    case TASK_TRIGGER_BOOT:
                        return "At system startup";
                    case TASK_TRIGGER_LOGON:
                        return "At log on";
                    case TASK_TRIGGER_SESSION_STATE_CHANGE:
                        return "On session state change";
                    default:
                        return "Custom trigger";
                    }
                }
            }
        }
        else
        {
            return std::format("{} triggers", triggerCount);
        }

        return "";
    }

    bool ScheduledTaskManager::SetTaskEnabled(const ScheduledTaskInfo *task, bool enabled)
    {
        if (!task)
            return false;

        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool bComInitialized = SUCCEEDED(hr);

        if (!bComInitialized && hr != RPC_E_CHANGED_MODE)
        {
            LogWin32ErrorCode("CoInitializeEx", hr, "initializing COM for Task Scheduler");
            return false;
        }

        bool success = false;

        // Create Task Service instance
        wil::com_ptr<ITaskService> pService;
        hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void **)&pService);

        if (FAILED(hr))
        {
            LogWin32ErrorCode("CoCreateInstance(TaskScheduler)", hr, "creating task service");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Connect to the Task Scheduler service
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::Connect", hr, "connecting to task scheduler");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Get the root folder
        wil::com_ptr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::GetFolder", hr, "getting root folder");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Get the task
        std::wstring taskPathW = utils::Utf8ToWide(task->GetPath());
        wil::com_ptr<IRegisteredTask> pTask;
        hr = pRootFolder->GetTask(_bstr_t(taskPathW.c_str()), &pTask);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskFolder::GetTask", hr, "task '{}'", task->GetPath());
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Set enabled state
        hr = pTask->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
        if (SUCCEEDED(hr))
        {
            spdlog::info("Task '{}' {} successfully", task->GetPath(), enabled ? "enabled" : "disabled");
            success = true;
        }
        else
        {
            LogWin32ErrorCode("IRegisteredTask::put_Enabled", hr, "task '{}'", task->GetPath());
        }

        if (bComInitialized)
            CoUninitialize();

        return success;
    }

    bool ScheduledTaskManager::RunTask(const ScheduledTaskInfo *task)
    {
        if (!task)
            return false;

        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool bComInitialized = SUCCEEDED(hr);

        if (!bComInitialized && hr != RPC_E_CHANGED_MODE)
        {
            LogWin32ErrorCode("CoInitializeEx", hr, "initializing COM for Task Scheduler");
            return false;
        }

        bool success = false;

        // Create Task Service instance
        wil::com_ptr<ITaskService> pService;
        hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void **)&pService);

        if (FAILED(hr))
        {
            LogWin32ErrorCode("CoCreateInstance(TaskScheduler)", hr, "creating task service");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Connect to the Task Scheduler service
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::Connect", hr, "connecting to task scheduler");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Get the root folder
        wil::com_ptr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::GetFolder", hr, "getting root folder");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Get the task
        std::wstring taskPathW = utils::Utf8ToWide(task->GetPath());
        wil::com_ptr<IRegisteredTask> pTask;
        hr = pRootFolder->GetTask(_bstr_t(taskPathW.c_str()), &pTask);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskFolder::GetTask", hr, "task '{}'", task->GetPath());
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Run the task
        wil::com_ptr<IRunningTask> pRunningTask;
        hr = pTask->Run(_variant_t(), &pRunningTask);
        if (SUCCEEDED(hr))
        {
            spdlog::info("Task '{}' started successfully", task->GetPath());
            success = true;
        }
        else
        {
            LogWin32ErrorCode("IRegisteredTask::Run", hr, "task '{}'", task->GetPath());
        }

        if (bComInitialized)
            CoUninitialize();

        return success;
    }

    bool ScheduledTaskManager::DeleteTask(const ScheduledTaskInfo *task)
    {
        if (!task)
            return false;

        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool bComInitialized = SUCCEEDED(hr);

        if (!bComInitialized && hr != RPC_E_CHANGED_MODE)
        {
            LogWin32ErrorCode("CoInitializeEx", hr, "initializing COM for Task Scheduler");
            return false;
        }

        bool success = false;

        // Create Task Service instance
        wil::com_ptr<ITaskService> pService;
        hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void **)&pService);

        if (FAILED(hr))
        {
            LogWin32ErrorCode("CoCreateInstance(TaskScheduler)", hr, "creating task service");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Connect to the Task Scheduler service
        hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::Connect", hr, "connecting to task scheduler");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Get the root folder
        wil::com_ptr<ITaskFolder> pRootFolder;
        hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("ITaskService::GetFolder", hr, "getting root folder");
            if (bComInitialized)
                CoUninitialize();
            return false;
        }

        // Delete the task
        std::wstring taskPathW = utils::Utf8ToWide(task->GetPath());
        hr = pRootFolder->DeleteTask(_bstr_t(taskPathW.c_str()), 0);
        if (SUCCEEDED(hr))
        {
            spdlog::info("Task '{}' deleted successfully", task->GetPath());
            success = true;
        }
        else
        {
            LogWin32ErrorCode("ITaskFolder::DeleteTask", hr, "task '{}'", task->GetPath());
        }

        if (bComInitialized)
            CoUninitialize();

        return success;
    }

} // namespace pserv
