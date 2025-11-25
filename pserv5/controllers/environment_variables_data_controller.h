#pragma once
#include <core/data_controller.h>
#include <models/environment_variable_info.h>

namespace pserv
{

    class EnvironmentVariablesDataController : public DataController
    {
    private:
        // Edit buffer for property editing
        struct EditBuffer
        {
            std::string name;
            std::string value;
        };
        EditBuffer m_editBuffer;
        DataObject *m_editingObject = nullptr;

#ifndef PSERV_CONSOLE_BUILD
        // Add variable dialog state
        bool m_bShowAddDialog{false};
        EnvironmentVariableScope m_addDialogScope{EnvironmentVariableScope::User};
        char m_addNameBuffer[256]{};
        char m_addValueBuffer[4096]{};
        std::string m_addDialogError;
#endif

    public:
        EnvironmentVariablesDataController();
        ~EnvironmentVariablesDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;

        // Property editing support
        void BeginPropertyEdits(DataObject *obj) override;
        bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue) override;
        bool CommitPropertyEdits(DataObject *obj) override;

#ifndef PSERV_CONSOLE_BUILD
        // Add variable dialog
        void ShowAddVariableDialog(EnvironmentVariableScope scope);
        void RenderAddVariableDialog(HWND hWnd);
#endif
    };

} // namespace pserv
