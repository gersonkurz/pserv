#pragma once
// Shared between the pserv_core_tests translation units.

namespace pserv
{
    class DataController;
}

namespace pserv::tests
{
    /// Records one check; prints "FAIL: <what>" if bOk is false.
    void Check(bool bOk, std::string_view what);

    /// refresh_tests.cpp
    void RunRefreshUnitTests();
    void RunRefreshStress(const std::vector<DataController *> &controllers, int rounds);
}
