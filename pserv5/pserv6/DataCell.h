#pragma once
#include "DataCell.g.h"

namespace winrt::pserv6::implementation
{
    struct DataCell : DataCellT<DataCell>
    {
        DataCell() = default;
        hstring Text() const { return m_text; }
        hstring FontName() const { return m_fontName; }
        void SetFontName(hstring const& name) { if (m_fontName != name) { m_fontName = name; Changed(L"FontName"); } }
        Windows::UI::Text::FontWeight FontWeight() const { return {m_weight}; }
        Windows::UI::Text::FontStyle FontStyle() const { return m_italic ? Windows::UI::Text::FontStyle::Italic : Windows::UI::Text::FontStyle::Normal; }
        void SetFontStyle(uint16_t weight, bool italic)
        {
            if (m_weight != weight) { m_weight = weight; Changed(L"FontWeight"); }
            if (m_italic != italic) { m_italic = italic; Changed(L"FontStyle"); }
        }
        double Width() const { return m_width; }
        Microsoft::UI::Xaml::TextAlignment Alignment() const { return m_alignment; }
        Microsoft::UI::Xaml::Media::Brush Foreground() const { return m_foreground; }
        void Update(hstring const& text, double width, Microsoft::UI::Xaml::TextAlignment alignment);
        void SetWidth(double width);
        void SetForeground(Microsoft::UI::Xaml::Media::Brush const& brush);
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return m_changed.add(handler); }
        void PropertyChanged(winrt::event_token const& token) noexcept { m_changed.remove(token); }
    private:
        hstring m_text;
        hstring m_fontName{L"Segoe UI Variable"};
        uint16_t m_weight{400};
        bool m_italic{false};
        double m_width{100};
        Microsoft::UI::Xaml::TextAlignment m_alignment{Microsoft::UI::Xaml::TextAlignment::Left};
        Microsoft::UI::Xaml::Media::Brush m_foreground{nullptr};
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_changed;
        void Changed(wchar_t const* name) { m_changed(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{name}); }
    };
}
namespace winrt::pserv6::factory_implementation
{
    struct DataCell : DataCellT<DataCell, implementation::DataCell> {};
}
