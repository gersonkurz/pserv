#pragma once
#include "section.h"
#include "typed_value.h"

namespace pserv::config {

class RootSettings : public Section {
public:
    RootSettings() : Section{} {}

    struct LoggingSettings : public Section {
        LoggingSettings(Section* pParent) : Section{pParent, "Logging"} {}
        TypedValue<std::string> logLevel{this, "LogLevel", "debug"};
    } logging{this};

    struct WindowSettings : public Section {
        WindowSettings(Section* pParent) : Section{pParent, "Window"} {}
        TypedValue<int32_t> width{this, "Width", 1280};
        TypedValue<int32_t> height{this, "Height", 720};
        TypedValue<int32_t> positionX{this, "PositionX", 100};
        TypedValue<int32_t> positionY{this, "PositionY", 100};
        TypedValue<bool> maximized{this, "Maximized", false};
    } window{this};

    struct ApplicationSettings : public Section {
        ApplicationSettings(Section* pParent) : Section{pParent, "Application"} {}
        TypedValue<std::string> activeView{this, "ActiveView", "Services"};
    } application{this};

    struct ServicesTableSettings : public Section {
        ServicesTableSettings(Section* pParent) : Section{pParent, "ServicesTable"} {}
        TypedValue<std::string> columnWidths{this, "ColumnWidths", "300,200,80,100,80"};
        TypedValue<std::string> columnOrder{this, "ColumnOrder", "0,1,2,3,4"};
        TypedValue<int32_t> sortColumn{this, "SortColumn", -1};
        TypedValue<bool> sortAscending{this, "SortAscending", true};
    } servicesTable{this};
};

extern RootSettings theSettings;

} // namespace pserv::config
