#pragma once
#include <core/data_object_column.h>
#include <core/data_object_container.h>

namespace pserv
{
    inline constexpr std::string_view SERVICES_DATA_CONTROLLER_NAME{"Services"};
    inline constexpr std::string_view DEVICES_DATA_CONTROLLER_NAME{"Devices"};
    inline constexpr std::string_view PROCESSES_DATA_CONTROLLER_NAME{"Processes"};
    inline constexpr std::string_view WINDOWS_DATA_CONTROLLER_NAME{"Windows"};
    inline constexpr std::string_view UNINSTALLER_DATA_CONTROLLER_NAME{"Uninstaller"};
    inline constexpr std::string_view STARTUP_PROGRAMS_DATA_CONTROLLER_NAME{"Startup Programs"};
    inline constexpr std::string_view NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME{"Network Connections"};
    inline constexpr std::string_view SCHEDULED_TASKS_DATA_CONTROLLER_NAME{"Scheduled Tasks"};
    inline constexpr std::string_view MODULES_DATA_CONTROLLER_NAME{"Modules"};
    inline constexpr std::string_view ENVIRONMENT_VARIABLES_CONTROLLER_NAME{"Environment Variables"};

    class DataAction;
    class DataObject;
    class DataActionDispatchContext;

    enum class VisualState
    {
        Normal,      // Default text color
        Highlighted, // Special highlight (e.g., running services, own processes)
        Disabled     // Grayed out (e.g., disabled services, inaccessible processes)
    };

    // Common actions available across all controllers (export/copy functionality)
    // Use negative IDs to avoid collision with controller-specific actions
    enum class CommonAction
    {
        Separator = -1,
        ExportToJson = -1000,
        CopyAsJson = -1001,
        ExportToTxt = -1002,
        CopyAsTxt = -1003
    };

    class DataPropertiesDialog;

    class DataController
    {
    public:
        DataController(std::string_view controllerName, std::string itemName, std::vector<DataObjectColumn> &&columns)
            : m_controllerName{std::move(controllerName)},
              m_itemName{std::move(itemName)},
              m_columns{std::move(columns)},
              m_pPropertiesDialog{nullptr}
        {
        }

        void ShowPropertiesDialog(DataActionDispatchContext &ctx);

        virtual ~DataController();

        // Core abstract methods
        virtual void Refresh() = 0;

        const DataObjectContainer &GetDataObjects() const
        {
            return m_objects;
        }

        virtual VisualState GetVisualState(const DataObject *service) const = 0;

        // Action system - new object-based interface
        virtual std::vector<const DataAction *> GetActions(const DataObject *dataObject) const = 0;

        void RenderPropertiesDialog();

        // Generic sort implementation using column metadata and GetTypedProperty()
        void Sort(int columnIndex, bool ascending);

        // Property editing transaction methods
        virtual void BeginPropertyEdits(DataObject *obj)
        {
            // Default: no-op
        }

        virtual bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue)
        {
            return false; // Default: not editable
        }

        virtual bool CommitPropertyEdits(DataObject *obj)
        {
            return false; // Default: no changes to commit
        }

        // Get combo box options for a specific column (only called if EditType is Combo)
        virtual std::vector<std::string> GetComboOptions(int columnIndex) const
        {
            return {}; // Default: no options
        }

        const std::vector<DataObjectColumn> &GetColumns() const
        {
            return m_columns;
        }
        const auto &GetControllerName() const
        {
            return m_controllerName;
        }
        const auto &GetItemName() const
        {
            return m_itemName;
        }
        bool IsLoaded() const
        {
            return m_bLoaded;
        }
        bool NeedsRefresh() const
        {
            return m_bNeedsRefresh;
        }
        void ClearRefreshFlag()
        {
            m_bNeedsRefresh = false;
        }

    protected:
        void Clear();

    protected:
        const std::string m_controllerName;
        const std::string m_itemName;
        const std::vector<DataObjectColumn> m_columns;
        DataObjectContainer m_objects;
        int m_lastSortColumn{-1};
        bool m_bLoaded{false};
        bool m_bNeedsRefresh{false};
        bool m_lastSortAscending{true};

    private:
        DataPropertiesDialog *m_pPropertiesDialog{nullptr};
    };
} // namespace pserv
