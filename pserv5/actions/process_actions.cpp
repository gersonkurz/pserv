#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_action.h>
#include <models/process_info.h>
#include <windows_api/process_manager.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>
#include <shellapi.h>

namespace pserv {

// Forward declare to avoid circular dependency
class ProcessesDataController;

namespace {

// Helper function to get ProcessInfo from DataObject
inline const ProcessInfo* GetProcessInfo(const DataObject* obj) {
	return static_cast<const ProcessInfo*>(obj);
}

// ============================================================================
// Properties Action
// ============================================================================

class ProcessPropertiesAction final : public DataAction {
public:
	ProcessPropertiesAction() : DataAction{"Properties", ActionVisibility::Both} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		// Will be implemented when we integrate with controllers
		// For now, this is a stub
		spdlog::info("Process Properties action executed (stub)");
	}
};

// ============================================================================
// File System Actions
// ============================================================================

class ProcessOpenLocationAction final : public DataAction {
public:
	ProcessOpenLocationAction() : DataAction{"Open File Location", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject* obj) const override {
		return !GetProcessInfo(obj)->GetPath().empty();
	}

	void Execute(DataActionDispatchContext& ctx) override {
		for (const auto* obj : ctx.m_selectedObjects) {
			const auto* proc = GetProcessInfo(obj);
			std::string path = proc->GetPath();
			if (!path.empty()) {
				// Select file in explorer
				std::wstring wPath = utils::Utf8ToWide(path);
				std::wstring cmd = L"/select,\"" + wPath + L"\"";
				ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL, SW_SHOW);
			}
		}
	}
};

// ============================================================================
// Priority Actions
// ============================================================================

class ProcessSetPriorityAction final : public DataAction {
private:
	DWORD m_priorityClass;

public:
	ProcessSetPriorityAction(std::string name, DWORD priorityClass)
		: DataAction{std::move(name), ActionVisibility::ContextMenu}
		, m_priorityClass{priorityClass}
	{}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<DWORD> pids;
		for (const auto* obj : ctx.m_selectedObjects) {
			pids.push_back(GetProcessInfo(obj)->GetPid());
		}

		// No async needed for priority usually, it's fast
		int success = 0;
		for (DWORD pid : pids) {
			if (ProcessManager::SetProcessPriority(pid, m_priorityClass)) {
				success++;
			}
		}
		spdlog::info("Set priority for {}/{} processes", success, pids.size());

		// Signal refresh needed
		// Note: This will be handled by the controller after we integrate
	}
};

// ============================================================================
// Terminate Action
// ============================================================================

class ProcessTerminateAction final : public DataAction {
public:
	ProcessTerminateAction() : DataAction{"Terminate Process", ActionVisibility::ContextMenu} {}

	bool IsAvailableFor(const DataObject*) const override {
		return true;
	}

	bool IsDestructive() const override {
		return true;
	}

	bool RequiresConfirmation() const override {
		return true;
	}

	void Execute(DataActionDispatchContext& ctx) override {
		std::vector<DWORD> pids;
		std::string confirmMsg = "Are you sure you want to terminate the following processes?\n\n";

		for (const auto* obj : ctx.m_selectedObjects) {
			const auto* proc = GetProcessInfo(obj);
			pids.push_back(proc->GetPid());
			confirmMsg += std::format("{} (PID: {})\n", proc->GetName(), proc->GetPid());
			if (pids.size() >= 10) {
				confirmMsg += "... and more\n";
				break;
			}
		}

		if (MessageBoxA(ctx.m_hWnd, confirmMsg.c_str(), "Confirm Termination", MB_YESNO | MB_ICONWARNING) == IDYES) {

			 // Async termination
			if (ctx.m_pAsyncOp) {
				ctx.m_pAsyncOp->Wait();
				delete ctx.m_pAsyncOp;
				ctx.m_pAsyncOp = nullptr;
			}

			ctx.m_pAsyncOp = new AsyncOperation();
			ctx.m_bShowProgressDialog = true;

			ctx.m_pAsyncOp->Start(ctx.m_hWnd, [pids](AsyncOperation* op) -> bool {
				size_t total = pids.size();
				size_t success = 0;
				for (size_t i = 0; i < total; ++i) {
					op->ReportProgress(static_cast<float>(i) / total, std::format("Terminating process PID {}...", pids[i]));
					if (ProcessManager::TerminateProcessById(pids[i])) {
						success++;
					}
				}
				op->ReportProgress(1.0f, std::format("Terminated {}/{} processes", success, total));
				return true;
			});
		}
	}
};

} // anonymous namespace

// ============================================================================
// Factory Function
// ============================================================================

std::vector<std::shared_ptr<DataAction>> CreateProcessActions() {
	return {
		std::make_shared<ProcessPropertiesAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ProcessOpenLocationAction>(),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: Realtime", REALTIME_PRIORITY_CLASS),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: High", HIGH_PRIORITY_CLASS),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: Above Normal", ABOVE_NORMAL_PRIORITY_CLASS),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: Normal", NORMAL_PRIORITY_CLASS),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: Below Normal", BELOW_NORMAL_PRIORITY_CLASS),
		std::make_shared<ProcessSetPriorityAction>("Set Priority: Low", IDLE_PRIORITY_CLASS),
		std::make_shared<DataActionSeparator>(),
		std::make_shared<ProcessTerminateAction>()
	};
}

} // namespace pserv
