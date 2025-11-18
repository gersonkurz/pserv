#pragma once
#include "../models/window_info.h"
#include <string>
#include <vector>

namespace pserv {

class WindowPropertiesDialog {
private:
    struct WindowPropertiesState {
        // Snapshot data
        HWND hwnd;
        std::string id;
        std::string title;
        std::string className;
        std::string style;
        std::string exStyle;
        std::string size;
        std::string position;
        std::string processId;
        std::string threadId;
        std::string processName;
        std::string windowId;

        // Original pointer (for identity only, not dereference)
        const WindowInfo* pOriginalWindow{nullptr};
    };

    bool m_bOpen{false};
    std::vector<WindowPropertiesState> m_windowStates;
    int m_activeTabIndex{0};

public:
    WindowPropertiesDialog() = default;
    ~WindowPropertiesDialog() = default;

    // Open the dialog for multiple windows (takes snapshot)
    void Open(const std::vector<WindowInfo*>& windows);

    // Close the dialog
    void Close();

    // Check if dialog is open
    bool IsOpen() const { return m_bOpen; }

    // Render the dialog
    void Render();

private:
    void RenderWindowContent(const WindowPropertiesState& state);
};

} // namespace pserv
