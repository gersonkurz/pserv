#pragma once
#include <core/data_object.h>

namespace pserv
{

    enum class WindowProperty
    {
        InternalID,
        Title,
        Class,
        Size,
        Position,
        Style,
        ExStyle,
        ID,
        ProcessID,
        ThreadID,
        Process
    };

    class WindowInfo : public DataObject
    {
    public:
        WindowInfo(HWND hwnd);
        virtual ~WindowInfo() = default;

        // DataObject implementation
        std::string GetProperty(int propertyId) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(WindowProperty::InternalID));
        }

        // Getters
        HWND GetHandle() const
        {
            return m_hwnd;
        }
        const std::string &GetTitle() const
        {
            return m_title;
        }
        const std::string &GetClassName() const
        {
            return m_className;
        }
        DWORD GetProcessId() const
        {
            return m_processId;
        }
        DWORD GetThreadId() const
        {
            return m_threadId;
        }
        DWORD GetStyle() const
        {
            return m_style;
        }
        DWORD GetExStyle() const
        {
            return m_exStyle;
        }
        DWORD GetWindowId() const
        {
            return m_windowId;
        }

        // Setters
        void SetTitle(std::string title)
        {
            m_title = std::move(title);
        }
        void SetClassName(std::string className)
        {
            m_className = std::move(className);
        }
        void SetRect(const RECT &rect)
        {
            m_rect = rect;
        }
        void SetStyle(DWORD style)
        {
            m_style = style;
        }
        void SetExStyle(DWORD exStyle)
        {
            m_exStyle = exStyle;
        }
        void SetWindowId(DWORD id)
        {
            m_windowId = id;
        }
        void SetProcessId(DWORD pid)
        {
            m_processId = pid;
        }
        void SetThreadId(DWORD tid)
        {
            m_threadId = tid;
        }
        void SetProcessName(std::string name)
        {
            m_processName = std::move(name);
        }

    private:
        HWND m_hwnd;
        std::string m_title;
        std::string m_className;
        RECT m_rect{0, 0, 0, 0};
        DWORD m_style{0};
        DWORD m_exStyle{0};
        DWORD m_windowId{0};
        DWORD m_processId{0};
        DWORD m_threadId{0};
        std::string m_processName;

        // Helpers
        std::string GetStyleString() const;
        std::string GetExStyleString() const;

        friend class WindowManager;
    };

} // namespace pserv
