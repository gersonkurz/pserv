/// @file data_controller.h
/// @brief Base class for all data view controllers.
///
/// DataController is the central abstraction for managing a collection of
/// DataObjects representing a specific system resource type (services, processes, etc.).
#pragma once
#include <core/data_object_column.h>
#include <core/data_object_container.h>

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

    /// @brief Everything a background refresh may read: a value copy taken on the UI thread, so the
    /// worker never reads controller state the UI can change while it runs (plan section 5.1).
    /// @brief Outcome of DataController::Publish().
    enum class PublishResult
    {
        Published, ///< The live objects now reflect this refresh.
        Stale,     ///< The request was invalidated meanwhile; nothing changed. Refresh again.
        Failed     ///< The enumeration or the merge failed. No live object was added or removed; if
                   ///< the merge failed, some surviving objects may already hold new values.
    };

    struct RefreshRequest
    {
        bool bAutoRefresh{false};
        uint64_t epoch{0};   ///< DataController's refresh epoch when the request was made.
        std::string target;  ///< What to enumerate (e.g. a remote machine name); empty = default.
        int sortColumn{-1};  ///< The sort to apply on the worker (-1 = none), so publishing need not sort.
        bool bSortAscending{true};
    };

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
        {
            LogColumnPropertyIdMismatches();
        }

        virtual ~DataController();

        /// @name Core Abstract Methods
        /// @{

        /// @brief Load or reload data from the system, synchronously on the calling thread:
        /// RefreshShadow() followed by Publish(). Used by frontends without a refresh worker.
        /// @param isAutoRefresh True if called by auto-refresh timer (may optimize behavior).
        void Refresh(bool isAutoRefresh = false);

        /// @name Background refresh (plan D5, section 5.1)
        /// The UI thread creates a request, a single refresh worker fills the controller's private
        /// shadow container with RefreshShadow(), and the UI thread publishes it with Publish(),
        /// which merges the shadow into the live objects. Only the UI thread touches the live
        /// objects; only the worker touches the shadow. RefreshCoordinator implements the protocol.
        /// @{

        /// @brief UI thread: capture the inputs of a refresh.
        RefreshRequest CreateRefreshRequest(bool isAutoRefresh) const
        {
            return RefreshRequest{isAutoRefresh, m_refreshEpoch, GetRefreshTarget(), m_lastSortColumn, m_lastSortAscending};
        }

        /// @brief Worker thread: enumerate into the shadow container. Reads only `request`, the
        /// shadow and immutable controller state. Errors are logged; Publish() then keeps the
        /// live objects unchanged.
        void RefreshShadow(const RefreshRequest &request);

        /// @brief UI thread: merge the shadow into the live objects, re-sort, mark loaded.
        /// @return Published; Stale if InvalidatePendingRefreshes() was called since the request
        ///         was created (nothing changed; request again); Failed if the enumeration or the
        ///         merge failed (no object added or removed; do not retry at once).
        PublishResult Publish(const RefreshRequest &request);

        /// @brief UI thread: make every refresh requested so far stale. Call on anything that
        /// must not be overwritten by an older enumeration: a committed edit, an action, a target
        /// change, a view switch.
        void InvalidatePendingRefreshes() { ++m_refreshEpoch; }
        /// @}

        /// @brief Determine the visual rendering state for an object.
        /// @param service The DataObject to evaluate.
        /// @return VisualState for row coloring (normal, highlighted, or disabled).
        virtual VisualState GetVisualState(const DataObject *service) const = 0;

        /// @brief Get available actions for a specific object.
        /// @param dataObject The object to get actions for (nullptr for global actions).
        /// @return Vector of action pointers (includes separators).
        virtual std::vector<const DataAction *> GetActions(const DataObject *dataObject) const = 0;
        /// @}

        /// @brief Get every action this controller can offer, regardless of object state.
        /// Used by the console frontend to register one subcommand per action.
        virtual std::vector<const DataAction *> GetAllActions() const { return {}; }

        /// @brief Index of the first column whose PropertyId differs from its index, or -1.
        /// Every consumer reads properties by column index, so this must be -1.
        int FindColumnPropertyIdMismatch() const;

        /// @brief Get read-only access to the data container.
        const DataObjectContainer &GetDataObjects() const { return m_objects; }

        /// @brief Check if this controller supports auto-refresh.
        /// @return true if periodic refresh is meaningful for this data type.
        virtual bool SupportsAutoRefresh() const { return true; }

        /// @brief Sort objects by a column.
        /// @param columnIndex Column index to sort by.
        /// @param ascending True for ascending, false for descending.
        void Sort(int columnIndex, bool ascending);

        /// @brief The column of the last Sort() (-1 = unsorted) and its direction.
        int GetLastSortColumn() const { return m_lastSortColumn; }
        bool IsLastSortAscending() const { return m_lastSortAscending; }

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

    private:
        void LogColumnPropertyIdMismatches() const;

    protected:
        /// @brief Enumerate this controller's objects into `target` (StartRefresh, enumerate,
        /// FinishRefresh). Runs on the refresh worker: it may read only `request`, `target` and
        /// state that never changes after construction. May throw.
        virtual void Collect(DataObjectContainer &target, const RefreshRequest &request) = 0;

        /// @brief The enumeration target captured into each RefreshRequest (e.g. remote machine).
        virtual std::string GetRefreshTarget() const { return {}; }

        /// @brief UI thread, after a successful Publish(): update UI-side derived state.
        virtual void OnPublished() {}

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

    private:
        uint64_t m_refreshEpoch{0};          ///< UI thread. See InvalidatePendingRefreshes().
        DataObjectContainer m_shadow;        ///< Worker thread only (between request and publish).
        std::string m_shadowTarget;          ///< Target the shadow currently holds objects for.
        bool m_bShadowValid{false};          ///< The last RefreshShadow() completed without error.
        bool m_bShadowSorted{false};         ///< ... and sorted the shadow by the request's sort.
    };
} // namespace pserv
