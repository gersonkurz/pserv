#include "precomp.h"
#include <core/property_edit_batch.h>
#include <iostream>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    struct Item final : pserv::DataObject
    {
        explicit Item(std::string id) : id(std::move(id)) {}
        std::string id, value{"original"};
        std::string GetStableID() const override { return id; }
        std::string GetItemName() const override { return id; }
        std::string GetProperty(int) const override { return value; }
        pserv::PropertyValue GetTypedProperty(int) const override { return value; }
        bool MatchesFilter(const std::string&) const override { return true; }
    };

    struct Controller final : pserv::DataController
    {
        Controller() : DataController("Test", "item", {
            {"Value", "value", pserv::ColumnDataType::String, true, pserv::ColumnEditType::Text},
            {"Read only", "readonly", pserv::ColumnDataType::String}})
        {
            for (auto id : {"A", "B", "C"}) m_objects.Append(new Item(id));
        }
        Item* editing{};
        std::string pending, fail;
        std::vector<std::string> committed;
        Item* Get(const std::string& id) { return m_objects.GetByStableId<Item>(id); }
        void Refresh(bool = false) override { throw std::runtime_error("No enumeration is allowed in this test"); }
        pserv::VisualState GetVisualState(const pserv::DataObject*) const override { return pserv::VisualState::Normal; }
        std::vector<const pserv::DataAction*> GetActions(const pserv::DataObject*) const override { return {}; }
        void BeginPropertyEdits(pserv::DataObject* object) override
        {
            editing = static_cast<Item*>(object);
            pending = editing->value;
        }
        bool SetPropertyEdit(pserv::DataObject* object, int column, const std::string& value) override
        {
            Require(object == editing, "Draft routed to another object");
            if (column != 0 || value == "reject") return false;
            pending = value;
            return true;
        }
        bool CommitPropertyEdits(pserv::DataObject* object) override
        {
            Require(object == editing, "Commit routed to another object");
            if (editing->id == fail) return false;
            editing->value = pending;
            committed.push_back(editing->id);
            return true;
        }
    };

    template<typename Action>
    void FailsWith(Action action, std::string_view message)
    {
        try { action(); }
        catch (const std::runtime_error& error)
        {
            Require(std::string_view(error.what()).find(message) != std::string_view::npos, "Failure omitted target/progress information");
            return;
        }
        throw std::runtime_error("Expected batch failure");
    }
}

int main() try
{
    using pserv::ApplyPropertyEditBatch;
    using pserv::PropertyEditBatchItem;
    auto neverCancel = [] { return false; };
    auto report = [](float, const std::string&) {};
    std::vector<PropertyEditBatchItem> batch{{"A", "First", {{0, "alpha"}}}, {"B", "Second", {{0, "beta"}}}, {"C", "Untouched", {}}};
    {
        Controller controller;
        Require(ApplyPropertyEditBatch(controller, batch, neverCancel, report), "Multi-object apply failed");
        Require(controller.Get("A")->value == "alpha" && controller.Get("B")->value == "beta", "Per-object drafts were mixed");
        Require(controller.Get("C")->value == "original" && controller.committed.size() == 2, "Unedited object was committed");
    }
    {
        Controller controller;
        auto missing = batch;
        missing[1].stableId = "gone";
        FailsWith([&] { ApplyPropertyEditBatch(controller, missing, neverCancel, report); }, "Second no longer exists");
        Require(controller.committed.empty(), "Missing target caused a partial write before preflight ended");
        auto invalid = batch;
        invalid[1].edits[0].first = 1;
        FailsWith([&] { ApplyPropertyEditBatch(controller, invalid, neverCancel, report); }, "not editable");
        Require(controller.committed.empty(), "Read-only edit caused a partial write");
        auto duplicate = batch;
        duplicate.push_back(batch[0]);
        FailsWith([&] { ApplyPropertyEditBatch(controller, duplicate, neverCancel, report); }, "Duplicate");
        Require(controller.committed.empty(), "Duplicate target was committed twice");
    }
    {
        Controller controller;
        controller.fail = "B";
        auto failedBatch = batch;
        failedBatch[2].edits = {{0, "gamma"}};
        FailsWith([&] { ApplyPropertyEditBatch(controller, failedBatch, neverCancel, report); }, "1 earlier item(s) applied; later items were not attempted.");
        Require(controller.committed.size() == 1 && controller.Get("A")->value == "alpha" && controller.Get("B")->value == "original", "Failed transaction crossed object boundaries");
        Require(controller.Get("C")->value == "original", "Batch continued writing after a failed commit");
    }
    {
        Controller controller;
        auto rejected = batch;
        rejected[0].edits[0].second = "reject";
        FailsWith([&] { ApplyPropertyEditBatch(controller, rejected, neverCancel, report); }, "0 earlier item(s) applied");
        Require(controller.committed.empty(), "Rejected value was committed");
        Require(!ApplyPropertyEditBatch(controller, batch, [] { return true; }, report), "Cancellation before apply ignored");
        Require(controller.committed.empty(), "Cancelled operation wrote data");
        Require(!ApplyPropertyEditBatch(controller, batch, [&] { return controller.committed.size() == 1; }, report), "Cancellation between objects ignored");
        Require(controller.committed.size() == 1 && controller.Get("B")->value == "original", "Cancellation changed the next object");
    }
    std::cout << "Property batch checks passed using in-memory objects only.\n";
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
