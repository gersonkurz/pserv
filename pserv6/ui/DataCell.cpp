#include "pch.h"
#include "DataCell.h"
#include "DataCell.g.cpp"

namespace winrt::pserv6::implementation
{
    void DataCell::Update(hstring const& text, double width, Microsoft::UI::Xaml::TextAlignment alignment)
    {
        if (m_text != text) { m_text = text; Changed(L"Text"); }
        SetWidth(width);
        if (m_alignment != alignment) { m_alignment = alignment; Changed(L"Alignment"); }
    }
    void DataCell::SetWidth(double width)
    {
        if (m_width != width) { m_width = width; Changed(L"Width"); }
    }
    void DataCell::SetForeground(Microsoft::UI::Xaml::Media::Brush const& brush)
    {
        if (m_foreground != brush) { m_foreground = brush; Changed(L"Foreground"); }
    }
}
