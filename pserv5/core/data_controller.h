#pragma once
#include "data_object.h"
#include "data_object_column.h"
#include <vector>
#include <memory>
#include "../core/async_operation.h"

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

	class DataController {
	protected:
		std::string m_controllerName;
		std::string m_itemName;
		const std::vector<DataObjectColumn> m_columns;

	public:
		DataController(std::string controllerName, std::string itemName, std::vector<DataObjectColumn>&& columns)
			: m_controllerName{ std::move(controllerName) }
			, m_itemName{ std::move(itemName) }
			, m_columns{ std::move(columns) }
		{
		}

		virtual ~DataController() = default;

		// Core abstract methods
		virtual void Refresh() = 0;
		virtual const std::vector<DataObject*>& GetDataObjects() const = 0;
		virtual VisualState GetVisualState(const DataObject* service) const = 0;
		virtual std::vector<int> GetAvailableActions(const DataObject* service) const = 0;
		virtual std::string GetActionName(int action) const = 0;
		virtual void DispatchAction(int action, DataActionDispatchContext& context) = 0;
		virtual void RenderPropertiesDialog() = 0;

		// Generic sort implementation using column metadata and GetTypedProperty()
		void Sort(int columnIndex, bool ascending);

		const std::vector<DataObjectColumn>& GetColumns() const { return m_columns; }
		const std::string& GetControllerName() const { return m_controllerName; }
		const std::string& GetItemName() const { return m_itemName; }
		bool IsLoaded() const { return m_bLoaded; }
		bool NeedsRefresh() const { return m_bNeedsRefresh; }
		void ClearRefreshFlag() { m_bNeedsRefresh = false; }

	protected:
		bool m_bLoaded{ false };
		bool m_bNeedsRefresh{ false };
		int m_lastSortColumn{ -1 };
		bool m_lastSortAscending{ true };
	};
}
