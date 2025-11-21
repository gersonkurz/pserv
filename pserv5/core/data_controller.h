#pragma once
#include <core/data_object.h>
#include <core/data_object_column.h>
#include <core/data_action.h>
#include <core/async_operation.h>

namespace pserv {
	class DataController;


	struct DataActionDispatchContext final
	{
		DataActionDispatchContext() = default;
		~DataActionDispatchContext()
		{
			if (m_pAsyncOp) {
				m_pAsyncOp->RequestCancel();
				m_pAsyncOp->Wait();
				delete m_pAsyncOp;
			}
		}
		HWND m_hWnd{ nullptr };
		AsyncOperation* m_pAsyncOp{ nullptr };  // Current async operation
		std::vector<const DataObject*> m_selectedObjects;  // Selected services for multi-select
		DataController* m_pController{ nullptr };  // Owning data controller
		bool m_bShowProgressDialog{ false };
	};

	enum class VisualState {
		Normal,      // Default text color
		Highlighted, // Special highlight (e.g., running services, own processes)
		Disabled     // Grayed out (e.g., disabled services, inaccessible processes)
	};

	// Common actions available across all controllers (export/copy functionality)
	// Use negative IDs to avoid collision with controller-specific actions
	enum class CommonAction {
		Separator = -1,
		ExportToJson = -1000,
		CopyAsJson = -1001,
		ExportToTxt = -1002,
		CopyAsTxt = -1003
	};

	class DataPropertiesDialog;

	class DataController {
	protected:
		const std::string m_controllerName;
		const std::string m_itemName;
		const std::vector<DataObjectColumn> m_columns;
		std::vector<DataObject*> m_objects;

	public:
		DataController(std::string controllerName, std::string itemName, std::vector<DataObjectColumn>&& columns)
			: m_controllerName{ std::move(controllerName) }
			, m_itemName{ std::move(itemName) }
			, m_columns{ std::move(columns) }
			, m_pPropertiesDialog{ nullptr }
		{
		}

		virtual ~DataController()
		{
			assert(m_pPropertiesDialog == nullptr);
			Clear();
		}

		// Core abstract methods
		virtual void Refresh() = 0;
		
		const std::vector<DataObject*>& GetDataObjects() const
		{
			return m_objects;
		}

		virtual VisualState GetVisualState(const DataObject* service) const = 0;

		// Action system - new object-based interface
		virtual std::vector<const DataAction*> GetActions(const DataObject* dataObject) const = 0;

		void RenderPropertiesDialog();

		// Generic sort implementation using column metadata and GetTypedProperty()
		void Sort(int columnIndex, bool ascending);

		// Common export/copy functionality
		void DispatchCommonAction(int action, DataActionDispatchContext& context);

		const std::vector<DataObjectColumn>& GetColumns() const { return m_columns; }
		const auto& GetControllerName() const { return m_controllerName; }
		const auto& GetItemName() const { return m_itemName; }
		bool IsLoaded() const { return m_bLoaded; }
		bool NeedsRefresh() const { return m_bNeedsRefresh; }
		void ClearRefreshFlag() { m_bNeedsRefresh = false; }

	protected:
		void Clear();

	protected:
		bool m_bLoaded{ false };
		bool m_bNeedsRefresh{ false };
		int m_lastSortColumn{ -1 };
		bool m_lastSortAscending{ true };

	private:
		DataPropertiesDialog* m_pPropertiesDialog{ nullptr };
	};
}
