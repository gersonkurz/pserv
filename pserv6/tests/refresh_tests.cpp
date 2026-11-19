// Phase 2 tests: background refresh (DataController shadow/publish, DataObjectContainer::MergeFrom,
// RefreshCoordinator). Called from pserv_core_tests.cpp.
#include "precomp.h"
#include <iostream>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/refresh_coordinator.h>
#include <utils/string_utils.h>
#include "test_support.h"

namespace pserv::tests
{
    namespace
    {
        enum class TestProperty
        {
            Id = 0,
            Value
        };

        // A minimal model: stable ID plus one value.
        // Fault injection: each counter, when >= 0, counts down per call and throws when it hits 0.
        struct Faults
        {
            static inline int stableId = -1;
            static inline int clone = -1;
            static inline int assign = -1;
            static void Reset() { stableId = clone = assign = -1; }
            static void Hit(int &counter, const char *what)
            {
                if (counter >= 0 && counter-- == 0)
                    throw std::runtime_error(std::string{"injected fault: "} + what);
            }
        };

        class TestItem final : public CloneableDataObject<TestItem>
        {
        public:
            static inline int s_alive = 0; ///< Live instances (Windows ASan has no leak checker).

            explicit TestItem(std::string id) : m_id{std::move(id)} { ++s_alive; }
            TestItem(const TestItem &other) : CloneableDataObject<TestItem>(other), m_id{other.m_id}, m_value{other.m_value} { ++s_alive; }
            TestItem &operator=(const TestItem &) = default;
            ~TestItem() override { --s_alive; }
            std::string m_id;
            std::string m_value;

            std::string GetStableID() const override
            {
                Faults::Hit(Faults::stableId, "GetStableID");
                return m_id;
            }
            DataObject *Clone() const override
            {
                Faults::Hit(Faults::clone, "Clone");
                return CloneableDataObject<TestItem>::Clone();
            }
            void AssignFrom(const DataObject &other) override
            {
                Faults::Hit(Faults::assign, "AssignFrom");
                CloneableDataObject<TestItem>::AssignFrom(other);
            }
            std::string GetProperty(int propertyId) const override { return propertyId == 0 ? m_id : m_value; }
            PropertyValue GetTypedProperty(int propertyId) const override { return GetProperty(propertyId); }
            bool MatchesFilter(const std::string &) const override { return true; }
            std::string GetItemName() const override { return m_id; }
        };

        // A controller whose "system" is a script: per target, the items and values an enumeration finds.
        class ScriptedController final : public DataController
        {
        public:
            ScriptedController()
                : DataController{"Scripted", "Item", {{TestProperty::Id, "Id", "Id"}, {TestProperty::Value, "Value", "Value"}}}
            {
            }

            std::map<std::string, std::vector<std::pair<std::string, std::string>>> m_script; ///< target -> (id, value)
            std::string m_target;
            int m_collectCount{0};
            bool m_bThrowInCollect{false};

            void SetTarget(std::string target)
            {
                m_target = std::move(target);
                InvalidatePendingRefreshes();
            }

            TestItem *Find(const std::string &id) const
            {
                for (auto *obj : GetDataObjects())
                {
                    if (obj->GetStableID() == id)
                        return static_cast<TestItem *>(obj);
                }
                return nullptr;
            }

            std::string ValueOf(const std::string &id) const
            {
                const auto *item = Find(id);
                return item ? item->m_value : "<absent>";
            }

            VisualState GetVisualState(const DataObject *) const override { return VisualState::Normal; }
            std::vector<const DataAction *> GetActions(const DataObject *) const override { return {}; }

        protected:
            std::string GetRefreshTarget() const override { return m_target; }

            void Collect(DataObjectContainer &target, const RefreshRequest &request) override
            {
                ++m_collectCount;
                if (m_bThrowInCollect)
                    throw std::runtime_error("injected enumeration failure");
                target.StartRefresh();
                for (const auto &[id, value] : m_script[request.target])
                {
                    auto *item = target.GetByStableId<TestItem>(id);
                    if (!item)
                        item = target.Append(DBG_NEW TestItem{id});
                    item->m_value = value;
                }
                target.FinishRefresh();
            }
        };

        // A hand-drained task queue: stands in for the worker thread and for the UI dispatcher.
        struct ManualQueue
        {
            std::deque<RefreshCoordinator::Task> tasks;
            void Post(RefreshCoordinator::Task task) { tasks.push_back(std::move(task)); }
            size_t RunAll()
            {
                size_t n = 0;
                while (!tasks.empty())
                {
                    auto task = std::move(tasks.front());
                    tasks.pop_front();
                    task();
                    ++n;
                }
                return n;
            }
        };

        struct Harness
        {
            ScriptedController controller;
            ManualQueue worker;
            ManualQueue ui;
            RefreshCoordinator coordinator{[this](auto task) { worker.Post(std::move(task)); }, [this](auto task) { ui.Post(std::move(task)); }};
            int published{0};

            Harness()
            {
                coordinator.m_onPublished = [this](DataController *) { ++published; };
            }
        };

        void TestMergeSemantics()
        {
            std::cout << "[merge: identity kept, values assigned, new cloned, missing released]\n";
            ScriptedController c;
            c.m_script[""] = {{"a", "1"}, {"b", "2"}};
            c.Refresh();
            TestItem *a = c.Find("a");
            TestItem *b = c.Find("b");
            Check(a && b && c.GetDataObjects().GetSize() == 2, "initial refresh did not publish a and b");
            b->Retain(REFCOUNT_DEBUG_ARGS); // keep b readable after it is removed

            c.m_script[""] = {{"c", "3"}, {"a", "10"}};
            c.Refresh();
            Check(c.Find("a") == a, "the surviving object 'a' changed identity");
            Check(a->m_value == "10", "the surviving object 'a' was not updated in place");
            Check(c.Find("c") != nullptr && c.ValueOf("c") == "3", "the new object 'c' was not published");
            Check(c.Find("b") == nullptr && c.GetDataObjects().GetSize() == 2, "the missing object 'b' is still published");
            Check(b->m_value == "2", "a removed but retained object became unreadable");
            b->Release(REFCOUNT_DEBUG_ARGS);
        }

        // Common shape of (a), (c), (e): something invalidates the controller between the worker
        // finishing and the UI publishing. The stale result must not be published; a new refresh
        // must be requested and then publish the current state.
        void ExpectStaleResultDiscarded(std::string_view scenario, const std::function<void(Harness &)> &mutate)
        {
            Harness h;
            h.controller.m_script[""] = {{"x", "old"}};
            h.controller.Refresh();

            h.coordinator.Request(&h.controller);
            h.worker.RunAll();                         // shadow now holds "old"
            mutate(h);                                 // e.g. the user edits x to "edited"
            const std::string liveAfterMutation = h.controller.ValueOf("x");
            h.ui.RunAll();                             // completion: must be discarded
            Check(h.published == 0, std::format("({}) a stale refresh was published", scenario));
            Check(h.controller.ValueOf("x") == liveAfterMutation,
                std::format("({}) live value changed by the stale refresh: '{}' -> '{}'", scenario, liveAfterMutation, h.controller.ValueOf("x")));
            Check(h.worker.tasks.size() == 1, std::format("({}) no new refresh was requested after discarding", scenario));
            h.worker.RunAll();
            h.ui.RunAll();
            Check(h.published == 1, std::format("({}) the follow-up refresh was not published", scenario));
            Check(h.controller.ValueOf("x") == h.controller.m_script[""][0].second, std::format("({}) follow-up did not publish the current state", scenario));
        }

        void TestInterleavings()
        {
            std::cout << "[refresh interleavings (plan Phase 2 a-f), stepped by hand]\n";

            // (a) an edit is committed between request and publish
            ExpectStaleResultDiscarded("a: edit committed", [](Harness &h)
                {
                    h.controller.Find("x")->m_value = "edited";        // the edit, applied to the live object
                    h.controller.InvalidatePendingRefreshes();          // what committing an edit must do
                    h.controller.m_script[""] = {{"x", "edited"}};     // the system now has the edited value
                    Check(h.controller.ValueOf("x") == "edited", "(a) setup");
                });

            // (b) the target changes mid-refresh: nothing of the old target may ever appear
            {
                Harness h;
                h.controller.m_script["A"] = {{"a1", "1"}};
                h.controller.m_script["B"] = {{"b1", "2"}};
                h.controller.SetTarget("A");
                h.controller.Refresh();
                Check(h.controller.Find("a1") != nullptr, "(b) setup: target A not published");
                h.coordinator.Request(&h.controller);
                h.worker.RunAll();          // collected for A
                h.controller.SetTarget("B"); // invalidates
                h.ui.RunAll();               // A's result discarded, B requested
                Check(h.published == 0, "(b) the old target's result was published");
                h.worker.RunAll();
                h.ui.RunAll();
                Check(h.published == 1 && h.controller.Find("b1") != nullptr, "(b) the new target was not published");
                Check(h.controller.Find("a1") == nullptr, "(b) an object of the old target is still published");
            }

            // (c) the view switches mid-refresh (the frontend invalidates on a view switch)
            ExpectStaleResultDiscarded("c: view switched", [](Harness &h) { h.controller.InvalidatePendingRefreshes(); });

            // (d) a result arrives while a dialog is open: discarded, then exactly one refresh on close
            {
                Harness h;
                h.controller.m_script[""] = {{"x", "old"}};
                h.controller.Refresh();
                h.coordinator.Request(&h.controller);
                h.worker.RunAll();
                h.coordinator.SetPaused(true); // a dialog opened
                h.controller.m_script[""] = {{"x", "new"}};
                h.ui.RunAll();
                Check(h.published == 0 && h.controller.ValueOf("x") == "old", "(d) a result was published while a dialog was open");
                Check(h.worker.tasks.empty(), "(d) a refresh started while a dialog was open");
                h.coordinator.Request(&h.controller, true); // auto-refresh tick while paused
                Check(h.worker.tasks.empty(), "(d) an auto-refresh started while a dialog was open");
                h.coordinator.SetPaused(false); // the dialog closed
                Check(h.worker.tasks.size() == 1, "(d) not exactly one refresh after the dialog closed");
                h.worker.RunAll();
                h.ui.RunAll();
                Check(h.published == 1 && h.controller.ValueOf("x") == "new", "(d) the refresh after closing did not publish");
            }

            // (e) an action completes mid-refresh (the frontend invalidates when an action completes)
            ExpectStaleResultDiscarded("e: action completed", [](Harness &h)
                {
                    h.controller.m_script[""] = {{"x", "after-action"}};
                    h.controller.InvalidatePendingRefreshes();
                });

            // (f) a selection retained for an open dialog survives a publish that removes its objects
            {
                Harness h;
                h.controller.m_script[""] = {{"k", "kept"}, {"other", "1"}};
                h.controller.Refresh();
                TestItem *k = h.controller.Find("k");
                k->Retain(REFCOUNT_DEBUG_ARGS); // the dialog's reference
                h.controller.m_script[""] = {{"other", "2"}};
                h.coordinator.Request(&h.controller);
                h.worker.RunAll();
                h.ui.RunAll();
                Check(h.published == 1 && h.controller.Find("k") == nullptr, "(f) the removed object is still published");
                Check(k->GetProperty(1) == "kept", "(f) the retained object became unreadable after removal");
                k->Release(REFCOUNT_DEBUG_ARGS);
            }

            // Coalescing: requests while one is in flight become exactly one follow-up refresh
            {
                Harness h;
                h.controller.m_script[""] = {{"x", "1"}};
                h.coordinator.Request(&h.controller);
                h.coordinator.Request(&h.controller);
                h.coordinator.Request(&h.controller);
                Check(h.worker.tasks.size() == 1, "(coalesce) more than one refresh in flight for one controller");
                h.worker.RunAll();
                h.ui.RunAll();
                Check(h.worker.tasks.size() == 1, "(coalesce) requests made while in flight did not produce one follow-up");
                h.worker.RunAll();
                h.ui.RunAll();
                Check(h.worker.tasks.empty() && h.published == 2 && h.controller.m_collectCount == 2, "(coalesce) unexpected follow-up count");
            }
        }

        // Thread-safe UI queue for the stress run: the worker posts, the main thread drains.
        struct UiQueue
        {
            std::mutex mutex;
            std::condition_variable ready;
            std::deque<RefreshCoordinator::Task> tasks;
            void Post(RefreshCoordinator::Task task)
            {
                {
                    std::lock_guard lock{mutex};
                    tasks.push_back(std::move(task));
                }
                ready.notify_one();
            }
            bool TryPop(RefreshCoordinator::Task &task)
            {
                std::lock_guard lock{mutex};
                if (tasks.empty())
                    return false;
                task = std::move(tasks.front());
                tasks.pop_front();
                return true;
            }
        };
    } // namespace

    namespace
    {
        // Every object in `c` must be valid (readable) and findable by its ID.
        void CheckContainerIntact(DataObjectContainer &c, size_t expected, std::string_view what)
        {
            Check(c.GetSize() == expected, std::format("{}: container size {} instead of {}", what, c.GetSize(), expected));
            for (auto *obj : c)
            {
                const auto id = obj->GetStableID();
                (void)obj->GetProperty(1); // reads freed memory if the object was released (ASan)
                Check(c.GetByStableId<TestItem>(id) == obj, std::format("{}: lookup lost '{}'", what, id));
            }
        }

        void TestExceptionSafety()
        {
            std::cout << "[container operations stay ownership-safe under injected exceptions]\n";
            const int aliveBefore = TestItem::s_alive;
            {
                // FinishRefresh: the 2nd GetStableID of a removed object throws.
                DataObjectContainer c;
                for (const char *id : {"a", "b", "c", "d"})
                    c.Append(DBG_NEW TestItem{id});
                c.StartRefresh();
                (void)c.GetByStableId<TestItem>("a");
                (void)c.GetByStableId<TestItem>("c"); // b and d unseen: to be removed
                Faults::stableId = 1;
                bool bThrew = false;
                try
                {
                    c.FinishRefresh();
                }
                catch (const std::exception &)
                {
                    bThrew = true;
                }
                Faults::Reset();
                Check(bThrew, "FinishRefresh: injected fault did not throw");
                CheckContainerIntact(c, 4, "FinishRefresh after fault");
                c.StartRefresh();
                (void)c.GetByStableId<TestItem>("a");
                c.FinishRefresh(); // now succeeds: only a survives
                CheckContainerIntact(c, 1, "FinishRefresh after recovery");
            }
            {
                // MergeFrom: the 2nd Clone throws; the 1st clone must not leak, live must be unchanged.
                DataObjectContainer live;
                live.Append(DBG_NEW TestItem{"a"});
                DataObjectContainer source;
                for (const char *id : {"a", "b", "c"})
                    source.Append(DBG_NEW TestItem{id});
                const int aliveBeforeMerge = TestItem::s_alive;
                Faults::clone = 1;
                bool bThrew = false;
                try
                {
                    live.MergeFrom(source);
                }
                catch (const std::exception &)
                {
                    bThrew = true;
                }
                Faults::Reset();
                Check(bThrew, "MergeFrom(Clone): injected fault did not throw");
                CheckContainerIntact(live, 1, "MergeFrom after Clone fault");
                Check(TestItem::s_alive == aliveBeforeMerge, std::format("MergeFrom after Clone fault: {} leaked clone(s)", TestItem::s_alive - aliveBeforeMerge));
                live.MergeFrom(source);
                CheckContainerIntact(live, 3, "MergeFrom after recovery");
            }
            {
                // MergeFrom: a fault *after* a successful clone (the stable ID of the next source
                // object throws). The clone already made must be released again.
                DataObjectContainer live;
                live.Append(DBG_NEW TestItem{"a"});
                DataObjectContainer source;
                for (const char *id : {"a", "b", "c"})
                    source.Append(DBG_NEW TestItem{id});
                const int aliveBeforeMerge = TestItem::s_alive;
                Faults::stableId = 2; // a: ok, b: ok (cloned), c: throws
                bool bThrew = false;
                try
                {
                    live.MergeFrom(source);
                }
                catch (const std::exception &)
                {
                    bThrew = true;
                }
                Faults::Reset();
                Check(bThrew, "MergeFrom(after clone): injected fault did not throw");
                CheckContainerIntact(live, 1, "MergeFrom after a fault following a clone");
                Check(TestItem::s_alive == aliveBeforeMerge,
                    std::format("MergeFrom after a fault following a clone: {} leaked clone(s)", TestItem::s_alive - aliveBeforeMerge));
            }
            {
                // MergeFrom: AssignFrom of a surviving object throws; nothing is released or lost.
                DataObjectContainer live;
                for (const char *id : {"a", "b"})
                    live.Append(DBG_NEW TestItem{id});
                DataObjectContainer source;
                source.Append(DBG_NEW TestItem{"b"});
                Faults::assign = 0;
                bool bThrew = false;
                try
                {
                    live.MergeFrom(source); // would remove 'a' if it completed
                }
                catch (const std::exception &)
                {
                    bThrew = true;
                }
                Faults::Reset();
                Check(bThrew, "MergeFrom(AssignFrom): injected fault did not throw");
                CheckContainerIntact(live, 2, "MergeFrom after AssignFrom fault");
            }
            {
                // Publish reports a failed merge as Failed; no live object is added or removed.
                ScriptedController c;
                c.m_script[""] = {{"x", "1"}};
                c.Refresh();
                c.m_script[""] = {{"x", "1"}, {"y", "2"}};
                const auto request = c.CreateRefreshRequest(false);
                c.RefreshShadow(request);
                Faults::clone = 0; // cloning the new 'y' into the live container throws
                const auto result = c.Publish(request);
                Faults::Reset();
                Check(result == PublishResult::Failed, "Publish: a failed merge was not reported as Failed");
                Check(c.GetDataObjects().GetSize() == 1 && c.Find("x") != nullptr, "Publish: live objects changed by a failed merge");
            }
            Check(TestItem::s_alive == aliveBefore, std::format("exception-safety tests leaked {} object(s)", TestItem::s_alive - aliveBefore));
        }

        void TestFailedRefreshIsNotPublished()
        {
            std::cout << "[a failing enumeration is reported as failed, never as published, and not retried in a loop]\n";
            Harness h;
            int failed = 0;
            h.coordinator.m_onFailed = [&failed](DataController *) { ++failed; };
            h.controller.m_script[""] = {{"x", "initial"}};
            h.controller.Refresh();
            h.controller.m_bThrowInCollect = true;
            h.controller.m_script[""] = {{"x", "never-seen"}};
            for (int i = 0; i < 3; ++i)
            {
                h.coordinator.Request(&h.controller, true);
                h.worker.RunAll();
                h.ui.RunAll();
            }
            Check(h.published == 0 && failed == 3, std::format("throwing controller: published {}x, failed {}x (expected 0 and 3)", h.published, failed));
            Check(h.worker.tasks.empty(), "throwing controller: a failure scheduled an automatic retry");
            Check(h.controller.ValueOf("x") == "initial", "throwing controller: live value changed");
            h.controller.m_bThrowInCollect = false;
            h.coordinator.Request(&h.controller);
            h.worker.RunAll();
            h.ui.RunAll();
            Check(h.published == 1 && h.controller.ValueOf("x") == "never-seen", "throwing controller: did not recover once enumeration succeeded");
        }

        struct SortOrderResult
        {
            size_t pairs{0};
            size_t outOfOrder{0};
            size_t compareFailures{0};
        };

        // Checks every adjacent pair of `column` against CompareStringEx (user locale,
        // LINGUISTIC_IGNORECASE: what the sort promises) in the controller's current direction.
        // A failed comparison is counted, never read as an ordering.
        SortOrderResult CheckSortOrder(const DataController &c, int column)
        {
            SortOrderResult result;
            const bool bAscending = c.IsLastSortAscending();
            std::wstring previous;
            bool bFirst = true;
            for (const auto *obj : c.GetDataObjects())
            {
                const PropertyValue value = obj->GetTypedProperty(column);
                std::wstring current = utils::Utf8ToWide(std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : obj->GetProperty(column));
                if (!bFirst)
                {
                    ++result.pairs;
                    const int cmp = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE, previous.c_str(), static_cast<int>(previous.size()),
                        current.c_str(), static_cast<int>(current.size()), nullptr, nullptr, 0);
                    if (cmp == 0)
                        ++result.compareFailures;
                    else if (bAscending ? cmp == CSTR_GREATER_THAN : cmp == CSTR_LESS_THAN)
                        ++result.outOfOrder;
                }
                previous = std::move(current);
                bFirst = false;
            }
            return result;
        }

        std::vector<std::string> IdsInOrder(const DataController &c)
        {
            std::vector<std::string> ids;
            for (const auto *obj : c.GetDataObjects())
                ids.push_back(obj->GetStableID());
            return ids;
        }

        struct FailingSortKey
        {
            static inline int calls = 0;
            static bool Function(const std::string &utf8, std::string &key)
            {
                ++calls;
                key = utf8;
                return utf8 != "B"; // one object's key fails
            }
        };

        void TestSortKeys()
        {
            std::cout << "[sort: key order matches CompareStringEx, empty strings sort first, a failed key keeps the order]\n";
            ScriptedController c;
            c.m_script[""] = {{"1", "b"}, {"2", ""}, {"3", "A"}, {"4", "a"}, {"5", "\xC3\xA4"}, {"6", "B"}, {"7", ""}, {"8", "a10"}, {"9", "a9"}};
            c.Refresh();
            for (const bool bAscending : {true, false})
            {
                c.Sort(1, bAscending);
                const auto r = CheckSortOrder(c, 1);
                Check(r.pairs == 8 && r.outOfOrder == 0 && r.compareFailures == 0,
                    std::format("sort keys ({}): {} pairs, {} out of order, {} failed comparisons", bAscending ? "ascending" : "descending", r.pairs, r.outOfOrder, r.compareFailures));
            }
            c.Sort(1, true);
            const auto ascending = IdsInOrder(c);
            Check(ascending.size() == 9 && c.ValueOf(ascending[0]).empty() && c.ValueOf(ascending[1]).empty(), "sort keys: empty strings do not sort first");

            // A failing key: the sort leaves the order as it is instead of mixing keys.
            DataObjectContainer::SetSortKeyFunctionForTesting(&FailingSortKey::Function);
            c.Sort(1, false);
            Check(IdsInOrder(c) == ascending, "sort keys: a failed key changed the order");

            // Through a refresh: the worker's sort fails, so Publish sorts again on the UI thread
            // (here failing again: the published order is the enumeration's, and nothing is lost).
            const RefreshRequest request = c.CreateRefreshRequest(false);
            c.RefreshShadow(request);
            const int callsBeforePublish = FailingSortKey::calls;
            Check(c.Publish(request) == PublishResult::Published, "sort keys: a failed worker sort prevented the publish");
            Check(FailingSortKey::calls > callsBeforePublish, "sort keys: Publish did not retry a failed worker sort");
            Check(c.GetDataObjects().GetSize() == 9, "sort keys: objects lost after a failed sort");

            DataObjectContainer::SetSortKeyFunctionForTesting(nullptr);
            c.Refresh(); // worker sort succeeds again (descending, the last requested sort)
            const auto r = CheckSortOrder(c, 1);
            Check(!c.IsLastSortAscending() && r.pairs == 8 && r.outOfOrder == 0 && r.compareFailures == 0, "sort keys: not sorted after the key function was restored");
        }
    } // namespace

    void RunRefreshUnitTests()
    {
        TestMergeSemantics();
        TestInterleavings();
        TestExceptionSafety();
        TestFailedRefreshIsNotPublished();
        TestSortKeys();
    }

    void RunRefreshStress(const std::vector<DataController *> &controllers, int rounds)
    {
        std::cout << std::format("[stress: real controllers refreshed on a RefreshWorker, {} round(s); UI thread reads every property meanwhile]\n", rounds);
        UiQueue ui;
        // Every view is sorted by its first column declared String. The direction alternates per
        // round and is set before the request, so the worker sorts. Every third round flips it again
        // after the request, so Publish must re-sort on the UI thread. The order is checked against
        // CompareStringEx after every single publish.
        std::map<const DataController *, int> sortColumn;
        std::map<const DataController *, std::string> sortedBy;
        for (auto *c : controllers)
        {
            const auto &columns = c->GetColumns();
            for (size_t i = 0; i < columns.size(); ++i)
            {
                if (columns[i].DataType == ColumnDataType::String)
                {
                    sortColumn[c] = static_cast<int>(i);
                    sortedBy[c] = columns[i].DisplayName;
                    break;
                }
            }
        }
        std::map<const DataController *, double> firstPublishMs;  // initial population: every object cloned
        std::map<const DataController *, double> maxWorkerSortMs; // later publishes, sorted on the worker
        std::map<const DataController *, double> maxUiResortMs;   // later publishes re-sorted on the UI thread
        std::map<const DataController *, int> published;
        SortOrderResult order;
        size_t orderChecks = 0;
        size_t reads = 0;
        {
            RefreshWorker worker;
            RefreshCoordinator coordinator{[&worker](auto task) { worker.Post(std::move(task)); }, [&ui](auto task) { ui.Post(std::move(task)); }};
            coordinator.m_onPublished = [&published](DataController *c) { ++published[c]; };

            for (int round = 0; round < rounds; ++round)
            {
                const bool bAscending = round % 2 == 0;
                const bool bFlipAfterRequest = round % 3 == 2;
                for (auto *c : controllers)
                {
                    if (sortColumn.contains(c))
                        c->Sort(sortColumn[c], bAscending);
                    coordinator.Request(c, true);
                    if (bFlipAfterRequest && sortColumn.contains(c))
                        c->Sort(sortColumn[c], !bAscending);
                }
                // Main thread = UI thread: read every property of every live object while the worker
                // refreshes the shadows, and run completions (publishes) as they arrive.
                for (;;)
                {
                    bool bAnyInFlight = false;
                    for (auto *c : controllers)
                        bAnyInFlight |= coordinator.IsInFlight(c);
                    RefreshCoordinator::Task task;
                    if (ui.TryPop(task))
                    {
                        // Each completion runs one Publish; attribute its time to the controller
                        // whose publish count changed, then check that controller's order.
                        const auto t0 = std::chrono::steady_clock::now();
                        std::map<const DataController *, int> before = published;
                        task();
                        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                        for (auto *c : controllers)
                        {
                            if (published[c] == before[c])
                                continue;
                            if (published[c] == 1)
                                firstPublishMs[c] = ms;
                            else if (bFlipAfterRequest)
                                maxUiResortMs[c] = std::max(maxUiResortMs[c], ms);
                            else
                                maxWorkerSortMs[c] = std::max(maxWorkerSortMs[c], ms);
                            if (sortColumn.contains(c))
                            {
                                const auto r = CheckSortOrder(*c, sortColumn[c]);
                                Check(r.outOfOrder == 0 && r.compareFailures == 0,
                                    std::format("stress round {}: {} sorted by '{}' {}: {} pair(s) out of order, {} failed comparison(s)", round, c->GetControllerName(),
                                        sortedBy[c], c->IsLastSortAscending() ? "ascending" : "descending", r.outOfOrder, r.compareFailures));
                                order.pairs += r.pairs;
                                order.outOfOrder += r.outOfOrder;
                                order.compareFailures += r.compareFailures;
                                ++orderChecks;
                            }
                        }
                        continue;
                    }
                    if (!bAnyInFlight)
                        break;
                    for (auto *c : controllers)
                    {
                        for (const auto *obj : c->GetDataObjects())
                        {
                            for (size_t i = 0; i < c->GetColumns().size(); ++i)
                            {
                                (void)obj->GetProperty(static_cast<int>(i));
                                (void)obj->GetTypedProperty(static_cast<int>(i));
                                ++reads;
                            }
                        }
                    }
                }
            }
        } // worker joins here

        for (auto *c : controllers)
        {
            Check(published[c] == rounds, std::format("stress: {} published {} of {} rounds", c->GetControllerName(), published[c], rounds));
            std::cout << std::format("  {}: {} objects, published {}x, sorted by '{}'; UI-thread publish: initial {:.2f} ms, "
                                     "later max {:.2f} ms (sorted on the worker), {:.2f} ms (re-sorted on the UI thread)\n",
                c->GetControllerName(), c->GetDataObjects().GetSize(), published[c], sortedBy[c], firstPublishMs[c], maxWorkerSortMs[c], maxUiResortMs[c]);
        }
        std::cout << std::format("  sort order checked after each of {} publishes: {} adjacent pairs, {} out of order, {} failed comparisons\n",
            orderChecks, order.pairs, order.outOfOrder, order.compareFailures);
        std::cout << std::format("  {} property reads on the UI thread during the run\n", reads);
    }
} // namespace pserv::tests
