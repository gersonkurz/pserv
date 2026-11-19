#include "precomp.h"
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <core/data_controller.h>
#include <models/module_info.h>

namespace pserv
{

    // Forward declare to avoid circular dependency
    class ModulesDataController;

    namespace
    {

        // Helper function to get ModuleInfo from DataObject
        inline const ModuleInfo *GetModuleInfo(const DataObject *obj)
        {
            return static_cast<const ModuleInfo *>(obj);
        }

        // ============================================================================
        // File System Actions
        // ============================================================================

        class ModuleOpenContainingFolderAction final : public DataAction
        {
        public:
            ModuleOpenContainingFolderAction()
                : DataAction{"Open Containing Folder", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                return !GetModuleInfo(obj)->GetPath().empty();
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                // Console: Not supported (requires GUI)
                spdlog::error("'Open Containing Folder' is not supported in console build");
                throw std::runtime_error("This action requires GUI and is not available in console mode");
#else
                for (const auto *obj : ctx.m_selectedObjects)
                {
                    const auto *module = GetModuleInfo(obj);
                    std::string path = module->GetPath();
                    if (!path.empty())
                    {
                        size_t lastSlash = path.find_last_of("\\/");
                        if (lastSlash != std::string::npos)
                        {
                            std::string folder = path.substr(0, lastSlash);
                            ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            spdlog::info("Opened containing folder for module: {}", module->GetName());
                        }
                        else
                        {
                            spdlog::warn("Could not determine containing folder for module: {}", module->GetName());
                        }
                    }
                    else
                    {
                        spdlog::warn("Module path is empty, cannot open containing folder: {}", module->GetName());
                    }
                }
#endif
            }
        };

        ModuleOpenContainingFolderAction theModuleOpenContainingFolderAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateModuleActions()
    {
        return {&theModuleOpenContainingFolderAction};
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllModuleActions()
    {
        // Console: Return all actions regardless of module state
        return {&theModuleOpenContainingFolderAction};
    }
#endif

} // namespace pserv
