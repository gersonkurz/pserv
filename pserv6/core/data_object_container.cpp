#include "precomp.h"
#include <core/refcount_interface.h>
#include <core/data_object.h>
#include <core/data_object_container.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>

namespace pserv
{
    DataObjectContainer::~DataObjectContainer()
    {
        Clear();
    }

    namespace
    {
        // A byte string that orders exactly like CompareStringEx(LOCALE_NAME_USER_DEFAULT,
        // LINGUISTIC_IGNORECASE) on the original text (LCMapStringEx with LCMAP_SORTKEY guarantees
        // this), so a sort compares bytes instead of converting and comparing UTF-16 per comparison.
        bool MakeSortKey(const std::string &utf8, std::string &key)
        {
            key.clear();
            if (utf8.empty())
            {
                // LCMapStringEx rejects empty input. The empty key orders before every non-empty
                // one, as CompareStringEx orders "" before any non-empty string.
                return true;
            }
            const std::wstring wide = utils::Utf8ToWide(utf8);
            constexpr DWORD flags = LCMAP_SORTKEY | LINGUISTIC_IGNORECASE;
            const int size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr, 0);
            if (size <= 0)
            {
                LogWin32Error("LCMapStringEx", "sort key size, {} characters", wide.size());
                return false;
            }
            key.resize(static_cast<size_t>(size));
            // For LCMAP_SORTKEY the destination is a byte buffer and its size is given in bytes.
            const int written = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, wide.c_str(), static_cast<int>(wide.size()),
                reinterpret_cast<LPWSTR>(key.data()), size, nullptr, nullptr, 0);
            if (written != size)
            {
                LogWin32Error("LCMapStringEx", "sort key, {} of {} bytes", written, size);
                key.clear();
                return false;
            }
            return true;
        }

        DataObjectContainer::SortKeyFunction g_pfnSortKey = &MakeSortKey;
    } // namespace

    void DataObjectContainer::SetSortKeyFunctionForTesting(SortKeyFunction pfn) noexcept
    {
        g_pfnSortKey = pfn ? pfn : &MakeSortKey;
    }

    bool DataObjectContainer::Sort(int columnIndex, bool ascending, ColumnDataType dataType)
    {
        // Keys are computed once per object, not once per comparison: the comparison-based version
        // converted both strings to UTF-16 and called CompareStringEx ~n log n times, ~100 ms for
        // the ~10,000 rows of the Modules view. The sort is stable, so rows with equal keys keep
        // their (refresh-stable) order instead of shuffling in the grid on every refresh.
        const bool bNumeric = dataType == ColumnDataType::Integer || dataType == ColumnDataType::UnsignedInteger || dataType == ColumnDataType::Size;

        struct Entry
        {
            uint64_t number{0};
            std::string key;
            DataObject *object{nullptr};
        };
        std::vector<Entry> entries;
        entries.reserve(m_vector.size());
        for (auto *dataObject : m_vector)
        {
            Entry entry;
            entry.object = dataObject;
            const PropertyValue value = dataObject->GetTypedProperty(columnIndex);
            if (bNumeric)
            {
                if (std::holds_alternative<uint64_t>(value))
                    entry.number = std::get<uint64_t>(value);
                else if (std::holds_alternative<int64_t>(value))
                    entry.number = static_cast<uint64_t>(std::get<int64_t>(value));
            }
            else
            {
                // String and Time columns: the typed string, or the display text if not a string.
                // One failed key would make the byte comparison meaningless for the whole sort, so
                // a failure keeps the current order.
                if (!g_pfnSortKey(std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : dataObject->GetProperty(columnIndex), entry.key))
                {
                    spdlog::error("Sort: no sort key for column {}; order left unchanged", columnIndex);
                    return false;
                }
            }
            entries.push_back(std::move(entry));
        }

        std::stable_sort(entries.begin(), entries.end(),
            [bNumeric, ascending](const Entry &a, const Entry &b)
            {
                if (bNumeric)
                    return ascending ? a.number < b.number : a.number > b.number;
                const int cmp = a.key.compare(b.key);
                return ascending ? cmp < 0 : cmp > 0;
            });

        for (size_t i = 0; i < entries.size(); ++i)
        {
            m_vector[i] = entries[i].object;
        }
        return true;
    }

    void DataObjectContainer::StartRefresh() noexcept
    {
        for (auto dataObject : m_vector)
        {
            dataObject->m_LastSeenGeneration = m_LastSeenGeneration;
        }
        InterlockedIncrement(&m_LastSeenGeneration);
    }

    // Both operations below run in two phases. Phase 1 does everything that can throw (allocating
    // vectors, computing stable IDs, cloning, assigning) without changing the container's membership.
    // Phase 2 only moves, erases and releases, none of which throws. An exception therefore never
    // leaves a released object in m_vector or m_lookup, never loses an object, and never leaks a
    // clone (a refresh catches the exception and the container lives on). This is an ownership
    // guarantee, not a transactional one: MergeFrom assigns surviving objects in place during phase
    // 1, so after a failure some of them may already hold new values.

    void DataObjectContainer::FinishRefresh()
    {
        // Phase 1 (may throw): split into kept and removed. One pass; this used to restart the
        // scan after every removal, which is quadratic when many objects disappear at once.
        std::vector<DataObject *> kept;
        std::vector<std::pair<std::string, DataObject *>> removed;
        kept.reserve(m_vector.size());
        for (auto *dataObject : m_vector)
        {
            if (dataObject->m_LastSeenGeneration == m_LastSeenGeneration)
                kept.push_back(dataObject);
            else
                removed.emplace_back(dataObject->GetStableID(), dataObject);
        }

        // Phase 2 (no-throw)
        m_vector = std::move(kept);
        for (auto &[stableId, dataObject] : removed)
        {
            m_lookup.erase(stableId);
            dataObject->Release(REFCOUNT_DEBUG_ARGS);
        }
    }

    void DataObjectContainer::MergeFrom(const DataObjectContainer &source)
    {
        // Releases the clones made so far unless phase 1 completes. Each clone is owned by this
        // guard from the instant Clone() returns: `created` is reserved for every possible clone
        // before the loop, so recording one cannot throw.
        struct CloneGuard
        {
            std::vector<DataObject *> created;
            bool bCommitted{false};
            ~CloneGuard()
            {
                if (bCommitted)
                    return;
                for (auto *clone : created)
                    clone->Release(REFCOUNT_DEBUG_ARGS);
            }
        } guard;

        std::vector<DataObject *> merged;
        std::unordered_map<std::string, DataObject *> lookup;
        std::vector<DataObject *> removed;
        {
            // Phase 1 (may throw). m_lookup and m_vector are only read. Surviving objects are
            // assigned in place: if an assignment throws, that object keeps a mix of old and new
            // values, but every object stays valid and owned exactly once.
            guard.created.reserve(source.m_vector.size());
            merged.reserve(source.m_vector.size());
            lookup.reserve(source.m_vector.size());
            for (const auto *src : source.m_vector)
            {
                std::string stableId{src->GetStableID()};
                DataObject *live = nullptr;
                const auto existing = m_lookup.find(stableId);
                if (existing != m_lookup.end())
                {
                    live = existing->second;
                    live->AssignFrom(*src);
                }
                else
                {
                    live = src->Clone();
                    guard.created.push_back(live); // within reserved capacity: no-throw
                }
                merged.push_back(live);
                lookup.emplace(std::move(stableId), live);
            }
            for (const auto &[stableId, old] : m_lookup)
            {
                if (!lookup.contains(stableId))
                    removed.push_back(old);
            }
        }
        guard.bCommitted = true; // the clones are now owned by `merged`

        // Phase 2 (no-throw)
        for (auto *live : merged)
        {
            live->m_LastSeenGeneration = m_LastSeenGeneration;
        }
        m_vector = std::move(merged);
        m_lookup = std::move(lookup);
        for (auto *old : removed)
        {
            old->Release(REFCOUNT_DEBUG_ARGS);
        }
    }

    DataObjectContainer::DataObjectContainer(const DataObjectContainer& copySrc)
    {
        // Copy constructor
        for (const auto& [key, dataObject] : copySrc.m_lookup)
        {
            assert(dataObject != nullptr);
            dataObject->Retain(REFCOUNT_DEBUG_ARGS);
            m_lookup[key] = dataObject;
        }
        m_vector = copySrc.m_vector;
    }

    DataObjectContainer& DataObjectContainer::operator=(const DataObjectContainer& copySrc)
    {
        if (this != &copySrc)
        {
            Clear();
            for (const auto& [key, dataObject] : copySrc.m_lookup)
            {
                assert(dataObject != nullptr);
                dataObject->Retain(REFCOUNT_DEBUG_ARGS);
                m_lookup[key] = dataObject;
            }
            m_vector = copySrc.m_vector;
        }
        return *this;
    }

    DataObjectContainer::DataObjectContainer(DataObjectContainer&& moveSrc)
    {
        m_lookup = std::move(moveSrc.m_lookup);
        m_vector = std::move(moveSrc.m_vector);
        moveSrc.m_lookup.clear();
        moveSrc.m_vector.clear();
    }

    DataObjectContainer& DataObjectContainer::operator=(DataObjectContainer&& moveSrc)
    {
        if (this != &moveSrc)
        {
            Clear();
            m_lookup = std::move(moveSrc.m_lookup);
            m_vector = std::move(moveSrc.m_vector);
            moveSrc.m_lookup.clear();
            moveSrc.m_vector.clear();
        }
        return *this;
    }

    

    void DataObjectContainer::Clear()
    {
        m_lookup.clear();
        for (const auto dataObject : m_vector)
        {
            assert(dataObject != nullptr);
            dataObject->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_vector.clear();
    }
} // namespace pserv
