# Auto-Refresh Implementation Plan

## Overview
Implement periodic auto-refresh of data controllers while preserving selection, maintaining stable pointers, and providing a silent, non-intrusive user experience.

## Prerequisites
- Object lifecycle refactoring (stable IDs, reference counting) COMPLETED ✓
- DataObjectContainer update-in-place pattern WORKING ✓

## Current Status (2025-11-22)

| Step | Status | Commit |
|------|--------|--------|
| STEP 1: Fix Selection Refcounting | ✅ COMPLETED | "Fix selection refcounting - STEP 1 of auto-refresh" |
| STEP 2: Add SupportsAutoRefresh | ✅ COMPLETED | "Add SupportsAutoRefresh to DataController - STEP 2 of auto-refresh" |
| STEP 3: Add Configuration | ✅ COMPLETED | "Add AutoRefresh configuration structure - STEP 3 of auto-refresh" |
| STEP 4: Add Timer Infrastructure | ✅ COMPLETED | "Add timer infrastructure for auto-refresh - STEP 4 of auto-refresh" |
| STEP 5: Implement Actual Refresh | ✅ COMPLETED | "Implement actual refresh - STEP 5 of auto-refresh" |
| STEP 6: Selection Cleanup | ✅ COMPLETED | Fixed update-in-place + cleanup logic |
| STEP 7: Pause During Edits | ✅ COMPLETED | "Add pause during property edits - STEP 7 of auto-refresh" |
| STEP 8: Status Bar Indicator | ✅ COMPLETED | "Add status bar indicator - STEP 8 of auto-refresh" |
| STEP 9: Menu Toggle | 🔄 AWAITING VERIFICATION | Code implemented, needs testing |
| STEP 10: Keyboard Shortcut | ⏳ PENDING | - |
| STEP 11: Interval Submenu | ⏳ PENDING | - |
| STEP 12: Scroll Preservation | ⏳ PENDING | - |

**RESUME POINT**: STEP 9 code implemented. View menu has "Auto-Refresh" item with Ctrl+R shortcut hint. User needs to compile and test: toggle via menu, verify setting persists across restarts, verify status bar updates.

---

## Implementation Steps

### **STEP 1: Fix Selection Refcounting (CRITICAL)**

**What**: Add Retain/Release discipline to m_selectedObjects management.

**Why**: Currently selected objects have no refcount ownership - they could be deleted while selected.

**Changes**:
1. Add destructor to `DataActionDispatchContext` that releases all selected objects
2. Find all locations in `main_window.cpp` that add to `m_selectedObjects` and add `Retain()` call
3. Find all locations that remove from `m_selectedObjects` and add `Release()` call
4. Find all locations that clear `m_selectedObjects` and add loop to release all

**Files**:
- `pserv5/core/data_action_dispatch_context.h` (add destructor declaration)
- `pserv5/core/data_action_dispatch_context.cpp` (implement destructor)
- `pserv5/main_window.cpp` (add Retain/Release calls around selection modifications)

**Verification**:
- Code compiles
- Selection still works (click items, multi-select with Ctrl/Shift)
- No crashes when closing properties dialog
- Memory leak detector shows no leaks (if enabled)

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 2: Add SupportsAutoRefresh to DataController**

**What**: Add virtual method for controllers to declare auto-refresh capability.

**Why**: Some views (Modules, Windows) shouldn't auto-refresh - controller knows best.

**Changes**:
1. Add `virtual bool SupportsAutoRefresh() const { return true; }` to `DataController` class
2. Override to return `false` in:
   - `ModulesDataController` (context-specific to a process)
   - `WindowsDataController` (too volatile/distracting)

**Files**:
- `pserv5/core/data_controller.h`
- `pserv5/controllers/modules_data_controller.h`
- `pserv5/controllers/windows_data_controller.h`

**Verification**:
- Code compiles
- No behavior change (method exists but not called yet)

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 3: Add AutoRefresh Configuration Structure**

**What**: Add auto-refresh settings to config system.

**Changes**:
1. Add `AutoRefreshSettings` struct to `settings.h`:
   ```cpp
   struct AutoRefreshSettings {
       bool enabled = false;
       uint32_t intervalMs = 2000;
       bool pauseDuringActions = true;
       bool pauseDuringEdits = true;
   };
   ```
2. Add `AutoRefreshSettings autoRefresh;` to main settings struct
3. Add persistence hooks (if not automatic)

**Files**:
- `pserv5/config/settings.h`

**Verification**:
- Code compiles
- Settings can be modified (in code, set `enabled = true`, verify it persists to TOML)
- No behavior change yet

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 4: Add Timer Infrastructure (No Refresh Yet)**

**What**: Add timer tracking and logging when refresh WOULD happen (but don't actually refresh).

**Changes**:
1. Add to `MainWindow` class:
   ```cpp
   std::chrono::steady_clock::time_point m_lastAutoRefreshTime;
   ```
2. Initialize in constructor
3. Add `ShouldAutoRefresh()` helper method with all checks
4. In `MainWindow::Render()`, add timer check that LOGS when it would refresh:
   ```cpp
   if (ShouldAutoRefresh()) {
       auto now = std::chrono::steady_clock::now();
       auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAutoRefreshTime);
       if (elapsed.count() >= theSettings.autoRefresh.intervalMs) {
           spdlog::info("Would auto-refresh now");  // Just log, don't refresh
           m_lastAutoRefreshTime = now;
       }
   }
   ```

**Files**:
- `pserv5/main_window.h`
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Enable auto-refresh in config (set `enabled = true`, `intervalMs = 2000`)
- Run app, check logs - should see "Would auto-refresh now" every 2 seconds
- Verify it pauses when:
  - Controller doesn't support it (switch to Modules or Windows view)
  - Async operation running
- No actual refresh happens yet

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 5: Implement Actual Auto-Refresh (No Selection Cleanup)**

**What**: Replace log message with actual refresh call.

**Changes**:
1. Replace `spdlog::info("Would auto-refresh now")` with:
   ```cpp
   if (m_pCurrentController) {
       spdlog::debug("Auto-refreshing {}", m_pCurrentController->GetControllerName());
       m_pCurrentController->Refresh();
   }
   ```

**Files**:
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Enable auto-refresh, watch Services view
- Services should update every 2 seconds (start/stop a service, watch it update)
- Selection behavior: selected items stay selected (refcounting keeps them alive)
- BUT: if you delete a service, it stays in selection (we'll fix this next)

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 6: Add Selection Cleanup for Removed Objects**

**What**: Remove objects from selection that no longer exist in container.

**Changes**:
1. After `Refresh()` call, add cleanup loop:
   ```cpp
   // Clean up selection: remove objects no longer in container
   const auto& container = m_pCurrentController->GetDataObjects();
   auto it = m_dispatchContext.m_selectedObjects.begin();
   while (it != m_dispatchContext.m_selectedObjects.end()) {
       auto* obj = *it;
       if (!container.GetByStableId<DataObject>(obj->GetStableID())) {
           obj->Release(REFCOUNT_DEBUG_ARGS);
           it = m_dispatchContext.m_selectedObjects.erase(it);
       } else {
           ++it;
       }
   }
   ```

**Files**:
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Enable auto-refresh, select a service
- Stop the service - selection should stay
- Delete/disable the service (if possible) - selection should clear automatically on next refresh
- Selection count in status bar should update correctly

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 7: Add Pause During Properties Dialog Edits**

**What**: Don't refresh while user has unsaved edits in properties dialog.

**Changes**:
1. Add to `DataPropertiesDialog`:
   ```cpp
   bool HasPendingEdits() const { return !m_pendingEdits.empty(); }
   ```
2. Add to `DataController`:
   ```cpp
   bool HasPropertiesDialogWithEdits() const {
       return m_pPropertiesDialog && m_pPropertiesDialog->HasPendingEdits();
   }
   ```
3. Add check in `ShouldAutoRefresh()`:
   ```cpp
   if (m_pCurrentController->HasPropertiesDialogWithEdits()) return false;
   ```

**Files**:
- `pserv5/dialogs/data_properties_dialog.h`
- `pserv5/core/data_controller.h`
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Open properties dialog, make an edit (don't apply)
- Auto-refresh should pause (check logs)
- Apply or cancel edit - auto-refresh resumes

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 8: Add Status Bar Indicator**

**What**: Show auto-refresh status in status bar.

**Changes**:
1. In status bar rendering code, add after other stats:
   ```cpp
   if (theSettings.autoRefresh.enabled) {
       ImGui::SameLine();
       ImGui::Text("| ⟳ Auto-refresh: %ums", theSettings.autoRefresh.intervalMs);
       if (!m_pCurrentController->SupportsAutoRefresh()) {
           ImGui::SameLine();
           ImGui::TextDisabled("(not supported for this view)");
       }
   }
   ```

**Files**:
- `pserv5/main_window.cpp` (status bar rendering section)

**Verification**:
- Code compiles
- Enable auto-refresh - status bar shows indicator
- Switch to Windows view - shows "not supported" message
- Disable auto-refresh - indicator disappears

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 9: Add Menu Toggle (Simple)**

**What**: Add menu item to enable/disable auto-refresh.

**Changes**:
1. In View menu (after view list), add:
   ```cpp
   ImGui::Separator();
   bool autoRefreshEnabled = theSettings.autoRefresh.enabled;
   if (ImGui::MenuItem("Auto-Refresh", "Ctrl+R", &autoRefreshEnabled)) {
       theSettings.autoRefresh.enabled = autoRefreshEnabled;
       theSettings.save(*m_pConfigBackend);
   }
   ```

**Files**:
- `pserv5/main_window.cpp` (View menu section)

**Verification**:
- Code compiles
- Toggle auto-refresh via menu - works
- Setting persists across restarts

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 10: Add Ctrl+R Keyboard Shortcut**

**What**: Add keyboard shortcut to toggle auto-refresh.

**Changes**:
1. In keyboard handling section (after Ctrl+Number shortcuts), add:
   ```cpp
   // Ctrl+R to toggle auto-refresh
   if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
       theSettings.autoRefresh.enabled = !theSettings.autoRefresh.enabled;
       theSettings.save(*m_pConfigBackend);
       spdlog::info("Auto-refresh {}", theSettings.autoRefresh.enabled ? "enabled" : "disabled");
   }
   ```

**Files**:
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Press Ctrl+R - auto-refresh toggles
- Menu checkmark updates
- Status bar updates

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 11: Add Interval Submenu**

**What**: Add submenu to change refresh interval.

**Changes**:
1. Replace simple MenuItem with BeginMenu/EndMenu:
   ```cpp
   if (ImGui::BeginMenu("Auto-Refresh")) {
       bool enabled = theSettings.autoRefresh.enabled;
       if (ImGui::MenuItem("Enabled", "Ctrl+R", &enabled)) {
           theSettings.autoRefresh.enabled = enabled;
           theSettings.save(*m_pConfigBackend);
       }
       ImGui::Separator();
       uint32_t intervals[] = {1000, 2000, 5000, 10000};
       const char* labels[] = {"Every 1 second", "Every 2 seconds", "Every 5 seconds", "Every 10 seconds"};
       for (int i = 0; i < 4; i++) {
           bool selected = theSettings.autoRefresh.intervalMs == intervals[i];
           if (ImGui::MenuItem(labels[i], nullptr, selected)) {
               theSettings.autoRefresh.intervalMs = intervals[i];
               theSettings.save(*m_pConfigBackend);
           }
       }
       ImGui::EndMenu();
   }
   ```

**Files**:
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Open View > Auto-Refresh submenu
- Change interval - takes effect immediately
- Status bar shows new interval

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

### **STEP 12: Add Scroll Position Preservation (Optional)**

**What**: Try to preserve scroll position during refresh.

**Changes**:
1. In auto-refresh code, wrap refresh with scroll save/restore:
   ```cpp
   float scrollY = ImGui::GetScrollY();
   m_pCurrentController->Refresh();
   // ... selection cleanup ...
   ImGui::SetScrollY(scrollY);
   ```

**Files**:
- `pserv5/main_window.cpp`

**Verification**:
- Code compiles
- Scroll down in a long list (Services)
- Enable auto-refresh
- Scroll position should stay stable
- If it jumps/flickers, REVERT this step and document limitation

**⚠️ STOP: Human must compile, test, verify, and approve before continuing.**

---

## Final Verification Checklist

After all steps complete:
- [ ] Auto-refresh can be enabled/disabled via menu and Ctrl+R
- [ ] Refresh interval can be changed (1s, 2s, 5s, 10s)
- [ ] Status bar shows auto-refresh status
- [ ] Selection preserved during refresh (same objects stay selected)
- [ ] Removed objects cleared from selection automatically
- [ ] Properties dialog works during refresh (same object, updated values)
- [ ] Refresh pauses during async operations
- [ ] Refresh pauses during property edits
- [ ] Modules and Windows views show "not supported" message
- [ ] Scroll position stable (or documented if not possible)
- [ ] Settings persist across restarts
- [ ] No crashes, no memory leaks
- [ ] Refresh is silent and non-intrusive

---

## Notes

- Each step is a potential commit point
- If any step fails, STOP and debug before continuing
- Some steps may reveal issues in previous steps - that's OK, fix and re-verify
- The refactoring completed earlier makes this all possible - without stable pointers, this would be impossible
