#include "pch.h"
#include "MainWindow.h"
#include "ExplorerPage.h"

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

MainWindow::MainWindow()
{
    InitializeComponent();
}

void MainWindow::InitializeComponent()
{
    Title(L"pserv6");
    SystemBackdrop(MicaBackdrop{});
    m_explorerPage = std::make_unique<ExplorerPage>();
    Content(m_explorerPage->Root());
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
