#pragma once
#include <models/installed_program_info.h>

namespace pserv
{

    class UninstallerPropertiesDialog
    {
    public:
        UninstallerPropertiesDialog() = default;
        ~UninstallerPropertiesDialog() = default;

        void Open(const std::vector<const InstalledProgramInfo *> &programs);
        bool Render(); // Returns true if changes were applied (for future use)
        bool IsOpen() const
        {
            return m_bIsOpen;
        }
        void Close();

    private:
        bool m_bIsOpen = false;
        std::vector<const InstalledProgramInfo *> m_programs; // Selected program(s) for properties

        void RenderSingleProgramProperties(const InstalledProgramInfo *program);
    };

} // namespace pserv
