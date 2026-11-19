#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_action.h>
#include <core/exporters/exporter_registry.h>
#include <utils/string_utils.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{
    int DataController::FindColumnPropertyIdMismatch() const
    {
        for (size_t i = 0; i < m_columns.size(); ++i)
        {
            if (m_columns[i].PropertyId != static_cast<int>(i))
                return static_cast<int>(i);
        }
        return -1;
    }

    void DataController::LogColumnPropertyIdMismatches() const
    {
        for (size_t i = 0; i < m_columns.size(); ++i)
        {
            if (m_columns[i].PropertyId != static_cast<int>(i))
            {
                spdlog::error("Controller '{}': column {} ('{}') shows property {}; columns must be in property-id order",
                    m_controllerName, i, m_columns[i].DisplayName, m_columns[i].PropertyId);
            }
        }
    }

    DataController::~DataController()
    {
        Clear();
    }

    void DataController::Sort(int columnIndex, bool ascending)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
            return;

        // Remember last sort for re-applying after refresh
        m_lastSortColumn = columnIndex;
        m_lastSortAscending = ascending;

        ColumnDataType dataType = m_columns[columnIndex].DataType;
        m_objects.Sort(columnIndex, ascending, dataType);
    }
    
    void DataController::Refresh(bool isAutoRefresh)
    {
        const RefreshRequest request = CreateRefreshRequest(isAutoRefresh);
        RefreshShadow(request);
        Publish(request);
    }

    void DataController::RefreshShadow(const RefreshRequest &request)
    {
        // Objects of another target (e.g. the previous remote machine) must never be published.
        if (request.target != m_shadowTarget)
        {
            m_shadow.Clear();
            m_shadowTarget = request.target;
        }
        try
        {
            m_bShadowSorted = false;
            Collect(m_shadow, request);
            // Sorting is the expensive part of a publish (key generation for every object), so it
            // runs here on the worker; MergeFrom adopts the shadow's order.
            if (request.sortColumn >= 0 && request.sortColumn < static_cast<int>(m_columns.size()))
            {
                m_bShadowSorted = m_shadow.Sort(request.sortColumn, request.bSortAscending, m_columns[request.sortColumn].DataType);
            }
            m_bShadowValid = true;
        }
        catch (const std::exception &e)
        {
            m_bShadowValid = false;
            spdlog::error("Failed to refresh {}: {}", m_controllerName, e.what());
        }
    }

    PublishResult DataController::Publish(const RefreshRequest &request)
    {
        if (request.epoch != m_refreshEpoch)
        {
            return PublishResult::Stale; // a mutation, target change or view switch happened meanwhile
        }
        if (!m_bShadowValid)
        {
            return PublishResult::Failed; // the enumeration failed: keep the live objects as they are
        }
        try
        {
            m_objects.MergeFrom(m_shadow); // ownership-safe on failure, see DataObjectContainer
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to publish {}: {}", m_controllerName, e.what());
            return PublishResult::Failed;
        }
        // The shadow was sorted on the worker with the sort current at request time; the UI thread
        // sorts only if the user changed the sort since then, or the worker's sort failed.
        if (m_lastSortColumn >= 0 &&
            (!m_bShadowSorted || m_lastSortColumn != request.sortColumn || m_lastSortAscending != request.bSortAscending))
        {
            Sort(m_lastSortColumn, m_lastSortAscending);
        }
        OnPublished();
        SetLoaded();
        return PublishResult::Published;
    }

    void DataController::Clear()
    {
        m_objects.Clear();
        m_bLoaded = false;
    }

} // namespace pserv
