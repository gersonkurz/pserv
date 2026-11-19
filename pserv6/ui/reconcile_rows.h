#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace pserv::winui
{
    // Keep the observable collection and all unchanged items in place. In particular,
    // an identical snapshot emits no collection events (no Reset/ReplaceAll).
    template<typename Collection, typename Row, typename Key>
    bool ReconcileRows(Collection const& current, std::vector<Row> const& desired, Key key)
    {
        // Bulk reorder: replace differing slots instead of repeatedly searching/moving
        // through the vector. Row identity survives; the caller restores selection by ID.
        uint32_t differences = 0;
        for (uint32_t i = 0; i < current.Size() && i < desired.size(); ++i)
            if (current.GetAt(i) != desired[i]) ++differences;
        if (differences > 16)
        {
            while (current.Size() > desired.size()) current.RemoveAt(current.Size() - 1);
            for (uint32_t i = 0; i < desired.size(); ++i)
            {
                if (i == current.Size()) current.InsertAt(i, desired[i]);
                else if (current.GetAt(i) != desired[i]) current.SetAt(i, desired[i]);
            }
            return true;
        }
        bool changed = false;
        std::unordered_set<std::string> wanted;
        for (auto const& row : desired) wanted.insert(key(row));
        for (uint32_t i = current.Size(); i > 0; --i)
            if (!wanted.contains(key(current.GetAt(i - 1)))) { current.RemoveAt(i - 1); changed = true; }

        for (uint32_t i = 0; i < desired.size(); ++i)
        {
            if (i < current.Size() && current.GetAt(i) == desired[i]) continue;
            for (uint32_t j = i + 1; j < current.Size(); ++j)
            {
                if (current.GetAt(j) == desired[i])
                {
                    current.RemoveAt(j);
                    break;
                }
            }
            current.InsertAt(i, desired[i]);
            changed = true;
        }
        return changed;
    }
}
