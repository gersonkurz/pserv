#pragma once
#include "data_object.h"
#include "data_object_column.h"
#include <vector>
#include <memory>

namespace pserv {
    class DataController {
    protected:
        std::string m_controllerName;
        std::string m_itemName;
        std::vector<DataObjectColumn> m_columns;

    public:
        DataController(std::string controllerName, std::string itemName)
            : m_controllerName(std::move(controllerName))
            , m_itemName(std::move(itemName))
        {
        }

        virtual ~DataController() = default;

        // Core abstract methods
        virtual void Refresh() = 0;
        virtual const std::vector<DataObjectColumn>& GetColumns() const = 0;
        virtual void Sort(int columnIndex, bool ascending) = 0;

        const std::string& GetControllerName() const { return m_controllerName; }
        const std::string& GetItemName() const { return m_itemName; }
    };
}
