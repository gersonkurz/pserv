#pragma once
// The properties dialog (plan Phase 5): every column of the selected object as a field, editable
// ones as text box, multi-line text box, number box or combo box (per the column's edit type),
// the object's properties-dialog actions as buttons, and OK applying the edits through the
// controller's edit transaction (BeginPropertyEdits / SetPropertyEdit / CommitPropertyEdits).
// With several objects selected, a picker at the top switches between them; edits are kept per
// object until OK.
#include "action_runner.h"

namespace pserv6
{
    class PropertiesDialog final : public std::enable_shared_from_this<PropertiesDialog>
    {
    public:
        /// @param bReadOnly No editors and no action buttons (a remote machine's objects).
        PropertiesDialog(pserv::DataController *pController, const std::vector<pserv::DataObject *> &objects, bool bReadOnly = false);

        std::string Title() const;

        /// Fill a (new) ContentDialog: content, OK/Cancel. Handlers hold only weak references.
        void Build(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const &dialog);

        struct ApplyOutcome
        {
            bool bChanged{false};
            std::vector<std::string> errors;
        };
        /// Commit the recorded edits, one transaction per object.
        ApplyOutcome ApplyEdits();

        /// The action button that closed the dialog, if any (run by the owner afterwards).
        const pserv::DataAction *RequestedAction() const { return m_pRequestedAction; }
        pserv::DataObject *CurrentObject() const;

    private:
        void ShowObject(size_t index);
        winrt::Microsoft::UI::Xaml::UIElement MakeField(size_t objectIndex, int column);
        void RecordEdit(size_t objectIndex, int column, std::string value);
        std::string ValueOf(size_t objectIndex, int column) const;

        pserv::DataController *m_pController;
        RetainedSelection m_objects;
        size_t m_current{0};
        bool m_bReadOnly{false};
        std::map<size_t, std::map<int, std::string>> m_edits; ///< object index -> column -> new value
        const pserv::DataAction *m_pRequestedAction{nullptr};
        winrt::weak_ref<winrt::Microsoft::UI::Xaml::Controls::ContentDialog> m_dialog;
        winrt::Microsoft::UI::Xaml::Controls::StackPanel m_fields{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::StackPanel m_actions{nullptr};
    };
} // namespace pserv6
