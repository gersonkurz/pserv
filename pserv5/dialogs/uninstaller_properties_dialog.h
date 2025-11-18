#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include "../models/installed_program_info.h"

namespace pserv {

class UninstallerPropertiesDialog {
public:
    UninstallerPropertiesDialog() = default;
    ~UninstallerPropertiesDialog() = default;

    void Open(const std::vector<InstalledProgramInfo*>& programs);
    bool Render(); // Returns true if changes were applied (for future use)
    bool IsOpen() const { return m_bIsOpen; }
    void Close();

private:
    bool m_bIsOpen = false;
    std::vector<InstalledProgramInfo*> m_programs; // Selected program(s) for properties

    void RenderSingleProgramProperties(InstalledProgramInfo* program);
}; 

} // namespace pserv
