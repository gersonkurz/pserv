#include "pserv6/reconcile_rows.h"
#include <memory>
#include <stdexcept>
#include <iostream>

struct Row { std::string id; };
using Item = std::shared_ptr<Row>;
struct Collection
{
    mutable std::vector<Item> items;
    mutable int notifications = 0;
    uint32_t Size() const { return static_cast<uint32_t>(items.size()); }
    Item GetAt(uint32_t i) const { return items.at(i); }
    void RemoveAt(uint32_t i) const { items.erase(items.begin() + i); ++notifications; }
    void InsertAt(uint32_t i, Item row) const { items.insert(items.begin() + i, row); ++notifications; }
};

int main()
{
    auto a = std::make_shared<Row>("a");
    auto b = std::make_shared<Row>("b");
    auto c = std::make_shared<Row>("c");
    Collection collection{{a, b, c}};
    auto check = [&](std::vector<Item> desired, int notifications)
    {
        collection.notifications = 0;
        pserv::winui::ReconcileRows(collection, desired, [](auto const& row) { return row->id; });
        if (collection.items != desired || collection.notifications != notifications)
            throw std::runtime_error("Incorrect row identity/order or collection notification count");
    };
    check({a, b, c}, 0); // A periodic unchanged snapshot must emit no collection events.
    check({a, c}, 1);    // Removal must leave surviving objects in place.
    check({a, b, c}, 1);
    check({c, a, b}, 2); // Sorting moves the existing object, preserving its identity.
    check({}, 3);
    check({b}, 1);
    check({b}, 0);
    std::cout << "WinUI row reconciliation checks passed.\n";
}
