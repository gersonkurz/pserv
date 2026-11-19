/// @file modules_data_controller.h
/// @brief Controller for loaded DLL/module enumeration.
///
/// Lists all modules (DLLs) loaded in the current process or a
/// selected target process.
#pragma once
#include <core/data_controller.h>

namespace pserv
{
    /// @brief Data controller for loaded modules (DLLs).
    ///
    /// Enumerates modules loaded in a process, displaying:
    /// - Module name and full path
    /// - Base address and size
    /// - Version information
    ///
    /// @note Auto-refresh is disabled because module lists are
    ///       context-specific to a selected process and rarely change.
    class ModulesDataController : public DataController
    {
    public:
        ModulesDataController();

    private:
        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
        bool SupportsAutoRefresh() const override { return false; }
    };

} // namespace pserv
