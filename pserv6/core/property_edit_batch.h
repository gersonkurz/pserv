#pragma once
#include <core/data_controller.h>

namespace pserv
{
    struct PropertyEditBatchItem
    {
        std::string stableId;
        std::string name;
        std::vector<std::pair<int, std::string>> edits;
    };

    // The caller owns a freshly enumerated controller for the entire operation.
    // Resolve every target before writing; edits may change an object's identity.
    // Each object is a separate transaction, not a system-wide atomic batch.
    template<typename Cancelled, typename Report>
    bool ApplyPropertyEditBatch(DataController& controller, const std::vector<PropertyEditBatchItem>& batch,
        Cancelled cancelled, Report report)
    {
        std::vector<std::pair<const PropertyEditBatchItem*, DataObject*>> targets;
        std::unordered_set<std::string> ids;
        for (auto const& item : batch)
        {
            if (item.edits.empty()) continue;
            if (cancelled()) return false;
            auto target = controller.GetDataObjects().GetByStableId<DataObject>(item.stableId);
            if (!target) throw std::runtime_error(std::format("{} no longer exists or could not be read. No changes applied.", item.name));
            if (!ids.insert(item.stableId).second) throw std::runtime_error("Duplicate property-edit target. No changes applied.");
            for (auto const& [column, value] : item.edits)
                if (column < 0 || column >= static_cast<int>(controller.GetColumns().size()) || !controller.GetColumns()[column].Editable)
                    throw std::runtime_error("A property is not editable. No changes applied.");
            targets.emplace_back(&item, target);
        }
        size_t applied = 0;
        for (auto const& [item, target] : targets)
        {
            if (cancelled()) return false;
            try
            {
                controller.BeginPropertyEdits(target);
                for (auto const& [column, value] : item->edits)
                    if (!controller.SetPropertyEdit(target, column, value))
                        throw std::runtime_error("A property value was rejected.");
                if (cancelled()) return false;
                report(static_cast<float>(applied) / targets.size(), std::format("Applying properties: {}", item->name));
                if (!controller.CommitPropertyEdits(target))
                    throw std::runtime_error("Could not apply properties. Check permissions and the diagnostic log.");
                ++applied;
            }
            catch (const std::exception& error)
            {
                throw std::runtime_error(std::format("{}: {} {} earlier item(s) applied; later items were not attempted.",
                    item->name, error.what(), applied));
            }
        }
        report(1.0f, std::format("Properties updated for {} item(s).", applied));
        return true;
    }
}
