#pragma once
#include <models/process_info.h>

namespace pserv {

class ProcessPropertiesDialog {
private:
    struct ProcessPropertiesState {
        // Snapshot data - copied on Open
        std::string name;
        DWORD pid{0};
        DWORD parentPid{0};
        std::string user;
        std::string path;
        std::string commandLine;
        std::string priority;
        
        // Stats
        std::string threadCount;
        std::string handleCount;
        std::string workingSet;
        std::string peakWorkingSet;
        std::string privateBytes;
        std::string virtualSize;
        std::string pagedPool;
        std::string nonPagedPool;
        std::string pageFaults;
        
        // Time
        std::string startTime;
        std::string totalCpuTime;
        std::string userCpuTime;
        std::string kernelCpuTime;

        // Original pointer (unsafe to use if controller refreshed, but kept for identity if needed)
        // We won't dereference this in Render()
        const ProcessInfo* pOriginalProcess{nullptr};
    };

    bool m_bOpen{false};
    std::vector<ProcessPropertiesState> m_processStates;
    int m_activeTabIndex{0};

public:
    ProcessPropertiesDialog() = default;
    ~ProcessPropertiesDialog() = default;

    // Open the dialog for multiple processes (takes snapshot)
    void Open(const std::vector<ProcessInfo*>& processes);

    // Close the dialog
    void Close();

    // Check if dialog is open
    bool IsOpen() const { return m_bOpen; }

    // Render the dialog
    void Render();

private:
    void RenderProcessContent(const ProcessPropertiesState& state);
};

} // namespace pserv
