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
};

extern RootSettings theSettings;

} // namespace pserv::config
