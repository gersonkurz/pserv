#include "pch.h"
#include "DataRow.h"
#include <iostream>

int main()
{
    // No XAML application, service enumeration, or system changes are needed.
    auto row = winrt::make<winrt::pserv6::implementation::DataRow>();
    auto cells = row.Cells();
    auto items = cells.try_as<winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Foundation::IInspectable>>();
    auto iterable = cells.try_as<winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Foundation::IInspectable>>();
    if (!items || !iterable)
    {
        std::cerr << "DataRow.Cells does not expose the collection interfaces required by XAML ItemsSource.\n";
        return 1;
    }
    cells.Append(nullptr);
    if (items.Size() != 1 || items.GetAt(0) != nullptr)
    {
        std::cerr << "The XAML interface does not view the same cell collection.\n";
        return 1;
    }
    std::cout << "DataRow.Cells exposes the XAML ItemsSource interfaces and shared contents.\n";
}
