#include "pch.h"
#include "MainWindow.h"

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace
{
    TextBlock MakeText(winrt::hstring const &text, double fontSize, winrt::hstring const &resourceKey)
    {
        TextBlock tb;
        tb.Text(text);
        tb.FontSize(fontSize);
        tb.TextWrapping(TextWrapping::Wrap);
        tb.Foreground(Application::Current().Resources().Lookup(winrt::box_value(resourceKey)).as<Brush>());
        return tb;
    }
}

MainWindow::MainWindow()
{
    InitializeComponent();
}

void MainWindow::InitializeComponent()
{
    Title(L"pserv6");
    SystemBackdrop(MicaBackdrop{});

    Grid root;
    root.RowDefinitions().Append(RowDefinition{});

    StackPanel layout;
    layout.Spacing(12);
    layout.Padding(ThicknessHelper::FromLengths(32, 32, 32, 32));

    auto title = MakeText(L"pserv6 bootstrap", 28.0, L"TextFillColorPrimaryBrush");
    title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

    auto subtitle = MakeText(
        L"WinUI 3 + C++/WinRT + CMake is wired up. The next real milestone is the generic explorer shell and the Services adapter.",
        15.0,
        L"TextFillColorSecondaryBrush");

    Border note;
    note.Padding(ThicknessHelper::FromLengths(16, 12, 16, 12));
    note.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    note.Background(Application::Current().Resources().Lookup(winrt::box_value(L"SubtleFillColorSecondaryBrush")).as<Brush>());
    note.Child(MakeText(
        L"Current scope: prove the build and runtime shell first, then adapt the existing pserv core without dragging ImGui-era window assumptions into the new UI.",
        14.0,
        L"TextFillColorPrimaryBrush"));

    layout.Children().Append(title);
    layout.Children().Append(subtitle);
    layout.Children().Append(note);

    root.Children().Append(layout);
    Content(root);
}

winrt::Microsoft::UI::Xaml::Markup::IXamlType
MainWindow::GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const &type)
{
    return m_provider.GetXamlType(type);
}

winrt::Microsoft::UI::Xaml::Markup::IXamlType
MainWindow::GetXamlType(winrt::hstring const &fullname)
{
    return m_provider.GetXamlType(fullname);
}

winrt::com_array<winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition>
MainWindow::GetXmlnsDefinitions()
{
    return m_provider.GetXmlnsDefinitions();
}
