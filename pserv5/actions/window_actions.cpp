#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_action.h>
#include <models/window_info.h>
#include <windows_api/window_manager.h>

namespace pserv {

// Forward declare to avoid circular dependency
class WindowsDataController;

namespace {

// Helper function to get WindowInfo from DataObject
inline const WindowInfo* GetWindowInfo(const DataObject* obj) {
	return static_cast<const WindowInfo*>(obj);
}

// ============================================================================
// Properties Action
// ============================================================================

class WindowPropertiesAction final : public DataAction {
public:
	WindowPropertiesAction() : DataAction{"Properties", ActionVisibility::Both} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		// Will be implemented when we integrate with controllers
		// For now, this is a stub
		spdlog::info("Window Properties action executed (stub)");
	}
};

// ============================================================================
// Window State Actions
// ============================================================================

class WindowShowAction final : public DataAction {
public:
	WindowShowAction() : DataAction{"Show", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::ShowWindow(hwnd, SW_SHOW)) {
				successCount++;
			}
		}
		spdlog::info("Showed {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

class WindowHideAction final : public DataAction {
public:
	WindowHideAction() : DataAction{"Hide", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::ShowWindow(hwnd, SW_HIDE)) {
				successCount++;
			}
		}
		spdlog::info("Hid {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

class WindowMinimizeAction final : public DataAction {
public:
	WindowMinimizeAction() : DataAction{"Minimize", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::ShowWindow(hwnd, SW_MINIMIZE)) {
				successCount++;
			}
		}
		spdlog::info("Minimized {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

class WindowMaximizeAction final : public DataAction {
public:
	WindowMaximizeAction() : DataAction{"Maximize", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::ShowWindow(hwnd, SW_MAXIMIZE)) {
				successCount++;
			}
		}
		spdlog::info("Maximized {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

class WindowRestoreAction final : public DataAction {
public:
	WindowRestoreAction() : DataAction{"Restore", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::ShowWindow(hwnd, SW_RESTORE)) {
				successCount++;
			}
		}
		spdlog::info("Restored {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

// ============================================================================
// Other Window Actions
// ============================================================================

class WindowBringToFrontAction final : public DataAction {
public:
	WindowBringToFrontAction() : DataAction{"Bring To Front", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::BringToFront(hwnd)) {
				successCount++;
			}
		}
		spdlog::info("Brought {}/{} windows to front", successCount, ctx.m_selectedObjects.size());
	}
};

class WindowCloseAction final : public DataAction {
public:
	WindowCloseAction() : DataAction{"Close", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	bool IsDestructive() const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		int successCount = 0;
		for (const auto* obj : ctx.m_selectedObjects) {
			HWND hwnd = GetWindowInfo(obj)->GetHandle();
			if (WindowManager::CloseWindow(hwnd)) {
				successCount++;
			}
		}
		spdlog::info("Closed {}/{} windows", successCount, ctx.m_selectedObjects.size());
	}
};

} // anonymous namespace

// ============================================================================
// Factory Function
// ============================================================================

std::vector<std::shared_ptr<DataAction>> CreateWindowActions() {
	return {
		std::make_shared<WindowPropertiesAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<WindowShowAction>(),
		std::make_shared<WindowHideAction>(),
		std::make_shared<WindowMinimizeAction>(),
		std::make_shared<WindowMaximizeAction>(),
		std::make_shared<WindowRestoreAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<WindowBringToFrontAction>(),
		std::make_shared<WindowCloseAction>()
	};
}

} // namespace pserv
