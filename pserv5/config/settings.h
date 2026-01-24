/// @file settings.h
/// @brief Application-wide configuration settings structure.
///
/// This file defines the complete configuration schema for pserv5,
/// including window state, display preferences, and per-table settings.
#pragma once

#include <config/section.h>
#include <config/typed_value.h>

namespace pserv
{
    namespace config
    {

        /// @brief Configuration section for table/grid display settings.
        ///
        /// Each data view (Services, Processes, etc.) has its own DisplayTable
        /// to store column widths, order, and sort preferences.
        struct DisplayTable : public Section
        {
            DisplayTable(Section *pParent, std::string name)
                : Section{pParent, name}
            {
            }

            /// @brief Comma-separated list of column widths in pixels.
            TypedValue<std::string> columnWidths{this, "ColumnWidths", "250,180,120,100,80,200,400,300,150,120,80,60,100,150,80,80,80,200"};

            /// @brief Comma-separated list of column indices defining display order.
            TypedValue<std::string> columnOrder{this, "ColumnOrder", "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17"};

            /// @brief Index of the column to sort by (-1 for no sort).
            TypedValue<int32_t> sortColumn{this, "SortColumn", -1};

            /// @brief Sort direction: true for ascending, false for descending.
            TypedValue<bool> sortAscending{this, "SortAscending", true};
        };

        /// @brief Root configuration section containing all application settings.
        ///
        /// RootSettings is the top-level Section that owns all configuration
        /// subsections. Use the global `theSettings` instance to access settings.
        ///
        /// @par Example Usage:
        /// @code
        /// // Read a setting
        /// int width = config::theSettings.window.width;
        ///
        /// // Write a setting
        /// config::theSettings.window.width.set(1920);
        /// config::theSettings.save(*backend);
        /// @endcode
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
                TypedValue<std::string> logFilePath{this, "LogFilePath", ""}; // Empty = use default (AppData)
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
                TypedValue<std::string> theme{this, "Theme", "Dark"};       // Dark, Light, TomorrowNightBlue, SunnyDay
                TypedValue<std::string> serviceMachineName{this, "ServiceMachineName", ""}; // Empty = local machine
            } application{this};

            struct AutoRefreshSettings : public Section
            {
                AutoRefreshSettings(Section *pParent)
                    : Section{pParent, "AutoRefresh"}
                {
                }
                TypedValue<bool> enabled{this, "Enabled", false};
                TypedValue<int32_t> intervalMs{this, "IntervalMs", 2000}; // 1-10 seconds
                TypedValue<bool> pauseDuringActions{this, "PauseDuringActions", true};
                TypedValue<bool> pauseDuringEdits{this, "PauseDuringEdits", true};
            } autoRefresh{this};

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
