#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_action_dispatch_context.h>
#include <models/window_info.h>
#include <windows_api/window_manager.h>

namespace pserv
{

    // Forward declare to avoid circular dependency
    class WindowsDataController;

    namespace
    {

        // Helper function to get WindowInfo from DataObject
        inline const WindowInfo *GetWindowInfo(const DataObject *obj)
        {
            return static_cast<const WindowInfo *>(obj);
        }

        // ============================================================================
        // Window State Actions
        // ============================================================================

        class WindowShowAction final : public DataAction
        {
        public:
            WindowShowAction()
                : DataAction{"Show", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::ShowWindow(hwnd, SW_SHOW))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Showed {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        class WindowHideAction final : public DataAction
        {
        public:
            WindowHideAction()
                : DataAction{"Hide", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::ShowWindow(hwnd, SW_HIDE))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Hid {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        class WindowMinimizeAction final : public DataAction
        {
        public:
            WindowMinimizeAction()
                : DataAction{"Minimize", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::ShowWindow(hwnd, SW_MINIMIZE))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Minimized {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        class WindowMaximizeAction final : public DataAction
        {
        public:
            WindowMaximizeAction()
                : DataAction{"Maximize", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::ShowWindow(hwnd, SW_MAXIMIZE))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Maximized {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        class WindowRestoreAction final : public DataAction
        {
        public:
            WindowRestoreAction()
                : DataAction{"Restore", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::ShowWindow(hwnd, SW_RESTORE))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Restored {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        // ============================================================================
        // Other Window Actions
        // ============================================================================

        class WindowBringToFrontAction final : public DataAction
        {
        public:
            WindowBringToFrontAction()
                : DataAction{"Bring To Front", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::BringToFront(hwnd))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Brought {}/{} windows to front", successCount, ctx.m_selectedObjects.size());
            }
        };

        class WindowCloseAction final : public DataAction
        {
        public:
            WindowCloseAction()
                : DataAction{"Close", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            bool IsDestructive() const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                int successCount = 0;
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    HWND hwnd = GetWindowInfo(obj)->GetHandle();
                    if (WindowManager::CloseWindow(hwnd))
                    {
                        successCount++;
                    }
                }
                spdlog::info("Closed {}/{} windows", successCount, ctx.m_selectedObjects.size());
            }
        };

        WindowShowAction theWindowShowAction;
        WindowHideAction theWindowHideAction;
        WindowMinimizeAction theWindowMinimizeAction;
        WindowMaximizeAction theWindowMaximizeAction;
        WindowRestoreAction theWindowRestoreAction;
        WindowBringToFrontAction theWindowBringToFrontAction;
        WindowCloseAction theWindowCloseAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateWindowActions()
    {
        return {
            &theWindowShowAction,
            &theWindowHideAction,
            &theWindowMinimizeAction,
            &theWindowMaximizeAction,
            &theWindowRestoreAction,
            &theDataActionSeparator,
            &theWindowBringToFrontAction,
            &theWindowCloseAction,
        };
    }

} // namespace pserv
