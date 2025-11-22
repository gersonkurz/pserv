#pragma once

#include <config/section.h>
#include <config/typed_value.h>

namespace pserv
{
    namespace config
    {

        struct DisplayTable : public Section
        {
            DisplayTable(Section *pParent, std::string name)
                : Section{pParent, name}
            {
            }
            // Default widths for 18 columns: DisplayName, Name, Status, StartType, ProcessId, ServiceType, BinaryPathName, Description, User, LoadOrderGroup,
            // ErrorControl, TagId, Win32ExitCode, ServiceSpecificExitCode, CheckPoint, WaitHint, ServiceFlags, ControlsAccepted
            TypedValue<std::string> columnWidths{this, "ColumnWidths", "250,180,120,100,80,200,400,300,150,120,80,60,100,150,80,80,80,200"};
            TypedValue<std::string> columnOrder{this, "ColumnOrder", "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17"};
            TypedValue<int32_t> sortColumn{this, "SortColumn", -1};
            TypedValue<bool> sortAscending{this, "SortAscending", true};
        };

        class RootSettings : public Section
        {
        public:
            RootSettings()
                : Section{}
            {
            }

            struct LoggingSettings : public Section
            {
                LoggingSettings(Section *pParent)
                    : Section{pParent, "Logging"}
                {
                }
                TypedValue<std::string> logLevel{this, "LogLevel", "debug"};
            } logging{this};

            struct WindowSettings : public Section
            {
                WindowSettings(Section *pParent)
                    : Section{pParent, "Window"}
                {
                }
                TypedValue<int32_t> width{this, "Width", 1280};
                TypedValue<int32_t> height{this, "Height", 720};
                TypedValue<int32_t> positionX{this, "PositionX", 100};
                TypedValue<int32_t> positionY{this, "PositionY", 100};
                TypedValue<bool> maximized{this, "Maximized", false};
            } window{this};

            struct ApplicationSettings : public Section
            {
                ApplicationSettings(Section *pParent)
                    : Section{pParent, "Application"}
                {
                }
                TypedValue<std::string> activeView{this, "ActiveView", "Services"};
                TypedValue<int32_t> fontSizeScaled{this, "FontSize", 1600}; // Font size * 100 (16.0f -> 1600)
                TypedValue<std::string> theme{this, "Theme", "Dark"};       // Dark, Light, or Classic
            } application{this};

            DisplayTable *getSectionFor(const std::string &name);

        private:
            DisplayTable servicesTable{this, "ServicesTable"};
            DisplayTable devicesTable{this, "DevicesTable"};
            DisplayTable processesTable{this, "ProcessesTable"};
            DisplayTable windowsTable{this, "WindowsTable"};
            DisplayTable modulesTable{this, "ModulesTable"};
            DisplayTable uninstallerTable{this, "UninstallerTable"};
            DisplayTable environmentVariablesTable{this, "EnvironmentVariablesTable"};
            DisplayTable startupProgramsTable{this, "StartupProgramsTable"};
            DisplayTable networkConnectionsProperties{this, "NetworkConnectionsProperties"};
            DisplayTable scheduledTasksProperties{this, "ScheduledTasksProperties"};
        };

        extern RootSettings theSettings;
    } // namespace config
} // namespace pserv::config
