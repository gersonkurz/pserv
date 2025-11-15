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
};

extern RootSettings theSettings;

} // namespace pserv::config
