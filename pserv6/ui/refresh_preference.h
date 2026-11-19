#pragma once

namespace pserv::winui
{
    // View capabilities affect presentation, never the user's global preference.
    class RefreshPreference
    {
    public:
        bool Enabled() const { return m_enabled; }
        void Enabled(bool value) { m_enabled = value; }
        void SupportsRefresh(bool value) { m_supported = value; }
        bool IsOn() const { return m_supported && m_enabled; }
        void UserToggled(bool value)
        {
            if (m_supported) m_enabled = value;
        }
    private:
        bool m_enabled{true};
        bool m_supported{false};
    };
}
