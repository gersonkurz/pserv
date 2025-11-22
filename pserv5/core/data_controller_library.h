#pragma once

namespace pserv
{
    class DataController;

    class DataControllerLibrary final
    {

    public:
        DataControllerLibrary() = default;
        ~DataControllerLibrary();

        // Core abstract methods
        const std::vector<DataController *> &GetDataControllers();
        void Clear();
    };
} // namespace pserv
