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
    void ReconcileRows(Collection const& current, std::vector<Row> const& desired, Key key)
    {
        std::unordered_set<std::string> wanted;
        for (auto const& row : desired) wanted.insert(key(row));
        for (uint32_t i = current.Size(); i > 0; --i)
            if (!wanted.contains(key(current.GetAt(i - 1)))) current.RemoveAt(i - 1);

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
        }
    }
}
