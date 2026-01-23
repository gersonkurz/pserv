/// @file data_controller.h
/// @brief Base class for all data view controllers.
///
/// DataController is the central abstraction for managing a collection of
/// DataObjects representing a specific system resource type (services, processes, etc.).
#pragma once
#include <core/data_object_column.h>
#include <core/data_object_container.h>
#ifdef PSERV_CONSOLE_BUILD
#include <argparse/argparse.hpp>
#endif


namespace pserv
{
    /// @name Controller Name Constants
    /// @brief Well-known names for built-in data controllers.
    /// @{
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
    /// @}

    class DataAction;
    class DataObject;
    class DataActionDispatchContext;

    /// @brief Visual rendering state for a data object row.
    enum class VisualState
    {
        Normal,      ///< Default text color.
        Highlighted, ///< Special highlight (e.g., running services, own processes).
        Disabled     ///< Grayed out (e.g., disabled services, inaccessible processes).
    };

    /// @brief IDs for common actions available across all controllers.
    /// Negative IDs avoid collision with controller-specific action indices.
    enum class CommonAction
    {
        Separator = -1,
        ExportToJson = -1000,
        CopyAsJson = -1001,
        ExportToTxt = -1002,
        CopyAsTxt = -1003
    };

#ifndef PSERV_CONSOLE_BUILD
    class DataPropertiesDialog;
#endif

    /// @brief Abstract base class for data view controllers.
    ///
    /// Each DataController manages a specific type of system resource:
    /// - Services, Devices, Processes, Windows, etc.
    ///
    /// Controllers are responsible for:
    /// - Defining column metadata for the data grid
    /// - Loading and refreshing data from the system
    /// - Providing actions available for their objects
    /// - Determining visual state for row rendering
    /// - Supporting property editing in the properties dialog
    ///
    /// @par Implementing a Custom Controller:
    /// @code
    /// class MyController : public DataController {
    /// public:
    ///     MyController() : DataController("MyItems", "Item", {
    ///         {"Name", "name", ColumnDataType::String},
    ///         {"Value", "value", ColumnDataType::Integer}
    ///     }) {}
    ///
    ///     void Refresh(bool isAutoRefresh) override { /* Load data */ }
    ///     VisualState GetVisualState(const DataObject*) const override {
    ///         return VisualState::Normal;
    ///     }
    ///     std::vector<const DataAction*> GetActions(const DataObject*) const override {
    ///         return { /* action list */ };
    ///     }
    /// };
    /// @endcode
    class DataController
    {
    public:
        /// @brief Construct a controller with name, item type, and columns.
        /// @param controllerName Unique name for this controller (e.g., "Services").
        /// @param itemName Singular name for items (e.g., "service") for UI labels.
        /// @param columns Column definitions for the data grid.
        DataController(std::string_view controllerName, std::string itemName, std::vector<DataObjectColumn> &&columns)
            : m_controllerName{std::move(controllerName)}
            , m_itemName{std::move(itemName)}
            , m_columns{std::move(columns)}
#ifndef PSERV_CONSOLE_BUILD
            , m_pPropertiesDialog{nullptr}
#endif
        {
        }

        virtual ~DataController();

#ifndef PSERV_CONSOLE_BUILD
        /// @brief Show the properties dialog for selected objects.
        void ShowPropertiesDialog(DataActionDispatchContext &ctx);
#endif

        /// @name Core Abstract Methods
        /// @{

        /// @brief Load or reload data from the system.
        /// @param isAutoRefresh True if called by auto-refresh timer (may optimize behavior).
        virtual void Refresh(bool isAutoRefresh = false) = 0;

        /// @brief Determine the visual rendering state for an object.
        /// @param service The DataObject to evaluate.
        /// @return VisualState for row coloring (normal, highlighted, or disabled).
        virtual VisualState GetVisualState(const DataObject *service) const = 0;

        /// @brief Get available actions for a specific object.
        /// @param dataObject The object to get actions for (nullptr for global actions).
        /// @return Vector of action pointers (includes separators).
        virtual std::vector<const DataAction *> GetActions(const DataObject *dataObject) const = 0;
        /// @}

#ifdef PSERV_CONSOLE_BUILD
        /// @brief Register CLI subcommands for this controller.
        virtual void RegisterArguments(argparse::ArgumentParser &program, std::vector<std::unique_ptr<argparse::ArgumentParser>> &subparsers) const;

        /// @brief Get all possible actions for CLI command registration.
        /// Override to return complete action set regardless of object state.
        virtual std::vector<const DataAction *> GetAllActions() const { return {}; }
#endif

        /// @brief Get read-only access to the data container.
        const DataObjectContainer &GetDataObjects() const { return m_objects; }

        /// @brief Check if this controller supports auto-refresh.
        /// @return true if periodic refresh is meaningful for this data type.
        virtual bool SupportsAutoRefresh() const { return true; }

#ifndef PSERV_CONSOLE_BUILD
        /// @brief Check if properties dialog has unsaved edits.
        /// Used to pause auto-refresh during editing.
        bool HasPropertiesDialogWithEdits() const;

        /// @brief Render the properties dialog if open.
        void RenderPropertiesDialog();
#endif

        /// @brief Sort objects by a column.
        /// @param columnIndex Column index to sort by.
        /// @param ascending True for ascending, false for descending.
        void Sort(int columnIndex, bool ascending);

        /// @name Property Editing Transaction
        /// Override these methods to support in-place editing in the properties dialog.
        /// @{

        /// @brief Begin an editing transaction for an object.
        virtual void BeginPropertyEdits(DataObject *obj) { }

        /// @brief Set a property value during editing.
        /// @return true if the value was accepted.
        virtual bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue) { return false; }

        /// @brief Commit all pending edits to the system.
        /// @return true if changes were successfully applied.
        virtual bool CommitPropertyEdits(DataObject *obj) { return false; }

        /// @brief Get dropdown options for a Combo-type column.
        virtual std::vector<std::string> GetComboOptions(int columnIndex) const { return {}; }
        /// @}

        /// @name Accessors
        /// @{
        const std::vector<DataObjectColumn> &GetColumns() const { return m_columns; }
        const auto &GetControllerName() const { return m_controllerName; }
        const auto &GetItemName() const { return m_itemName; }
        bool IsLoaded() const { return m_bLoaded; }
        bool NeedsRefresh() const { return m_bNeedsRefresh; }
        void ClearRefreshFlag() { m_bNeedsRefresh = false; }
        std::chrono::system_clock::time_point GetLastRefreshTime() const { return m_lastRefreshTime; }
        /// @}

    protected:
        /// @brief Clear all data objects from the container.
        void Clear();

        /// @brief Mark the controller as loaded and record the refresh timestamp.
        /// Call this at the end of a successful Refresh() implementation.
        void SetLoaded()
        {
            m_bLoaded = true;
            m_lastRefreshTime = std::chrono::system_clock::now();
        }

    protected:
        const std::string m_controllerName;              ///< Controller identifier.
        const std::string m_itemName;                    ///< Singular item name for UI.
        const std::vector<DataObjectColumn> m_columns;   ///< Column definitions.
        DataObjectContainer m_objects;                   ///< Data storage.
        int m_lastSortColumn{-1};                        ///< Last sorted column index.
        bool m_bLoaded{false};                           ///< True after first successful load.
        bool m_bNeedsRefresh{false};                     ///< True if data needs reloading.
        bool m_lastSortAscending{true};                  ///< Last sort direction.
        std::chrono::system_clock::time_point m_lastRefreshTime{}; ///< Time of last successful refresh.

#ifndef PSERV_CONSOLE_BUILD
    private:
        DataPropertiesDialog *m_pPropertiesDialog{nullptr}; ///< Active properties dialog.
#endif
    };
} // namespace pserv
