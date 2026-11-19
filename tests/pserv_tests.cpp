#include "precomp.h"

#include <config/settings.h>
#include <config/toml_backend.h>
#include <controllers/devices_data_controller.h>
#include <controllers/environment_variables_data_controller.h>
#include <controllers/services_data_controller.h>
#include <core/data_action.h>
#include <core/data_object_container.h>
#include <models/environment_variable_info.h>
#include <models/service_info.h>
#include <theme/base16_theme.h>
#include <utils/string_utils.h>
#include <windows_api/environment_variable_manager.h>

#include <iostream>

using namespace pserv;

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char *expr, int line)
    {
        if (!condition)
        {
            std::cerr << "FAIL (line " << line << "): " << expr << "\n";
            ++g_failures;
        }
    }

#define CHECK(expr) Check((expr), #expr, __LINE__)

    class DummyObject final : public DataObject
    {
    public:
        DummyObject(std::string id, int &liveCounter)
            : m_id{std::move(id)},
              m_liveCounter{liveCounter}
        {
            ++m_liveCounter;
        }
        ~DummyObject() override
        {
            --m_liveCounter;
        }
        std::string GetProperty(int) const override { return m_id; }
        PropertyValue GetTypedProperty(int) const override { return m_id; }
        bool MatchesFilter(const std::string &f) const override { return m_id.find(f) != std::string::npos; }
        std::string GetStableID() const override { return m_id; }
        std::string GetItemName() const override { return m_id; }

    private:
        std::string m_id;
        int &m_liveCounter;
    };

    void TestServicePropertyEditAndRemoteActions()
    {
        std::cout << "[Test] Service property editing & remote action filtering...\n";
        ServicesDataController svcCtrl;
        DevicesDataController devCtrl;
        DataController &svcDc = svcCtrl;

        ServiceInfo svc("NonExistentTestService_pserv6_unit");
        svc.SetValues("Initial Display", SERVICE_STOPPED, SERVICE_WIN32_OWN_PROCESS);
        svc.SetStartType(SERVICE_DEMAND_START);
        svc.SetBinaryPathName("C:\\Windows\\System32\\svchost.exe");
        svc.SetDescription("Initial Desc");

        // Column 0 is Name (read-only), Column 1 is Display Name (editable).
        svcDc.BeginPropertyEdits(&svc);
        CHECK(!svcDc.SetPropertyEdit(&svc, 0, "ShouldFailNameEdit"));
        CHECK(svcDc.SetPropertyEdit(&svc, 1, "Updated Display"));
        CHECK(svcDc.SetPropertyEdit(&svc, 3, "Automatic"));
        CHECK(!svcDc.SetPropertyEdit(&svc, 3, "BogusStartType"));
        CHECK(svcDc.SetPropertyEdit(&svc, 6, "C:\\Windows\\System32\\svchost.exe -k netsvcs"));
        CHECK(svcDc.SetPropertyEdit(&svc, 7, "Updated Desc"));

        // Committing against a non-existent service MUST fail and MUST NOT mutate local ServiceInfo.
        CHECK(!svcDc.CommitPropertyEdits(&svc));
        CHECK(svc.GetDisplayName() == "Initial Display");
        CHECK(svc.GetDescription() == "Initial Desc");

        // Local mode includes local-only actions.
        auto localActions = svcDc.GetActions(&svc);
        auto hasActionNamed = [](const std::vector<const DataAction *> &actions, std::string_view name)
        {
            for (const auto *a : actions)
            {
                if (a && !a->IsSeparator() && a->GetName() == name)
                    return true;
            }
            return false;
        };
        CHECK(hasActionNamed(localActions, "Start"));
        CHECK(hasActionNamed(localActions, "Open in Registry Editor"));
        CHECK(hasActionNamed(localActions, "Open in Explorer"));
        CHECK(hasActionNamed(localActions, "Open Terminal Here"));
        CHECK(hasActionNamed(localActions, "Delete Registry Key"));

        // Remote mode on BOTH Services and Devices strips local-only actions while preserving remote SCM actions.
        svcCtrl.SetMachineName("REMOTE_TEST_HOST");
        devCtrl.SetMachineName("REMOTE_TEST_HOST");
        CHECK(svcCtrl.GetMachineName() == "REMOTE_TEST_HOST");
        CHECK(devCtrl.GetMachineName() == "REMOTE_TEST_HOST");

        for (DataController *ctrl : std::initializer_list<DataController *>{&svcCtrl, &devCtrl})
        {
            auto remoteActions = ctrl->GetActions(&svc);
            CHECK(hasActionNamed(remoteActions, "Start"));
            CHECK(hasActionNamed(remoteActions, "Uninstall Service"));
            CHECK(!hasActionNamed(remoteActions, "Open in Registry Editor"));
            CHECK(!hasActionNamed(remoteActions, "Open in Explorer"));
            CHECK(!hasActionNamed(remoteActions, "Open Terminal Here"));
            CHECK(!hasActionNamed(remoteActions, "Delete Registry Key"));
        }
    }

    void TestEnvironmentVariableSafety()
    {
        std::cout << "[Test] Environment variable validation, REG_EXPAND_SZ preservation & safe rename...\n";

        CHECK(!EnvironmentVariableManager::IsValidVariableName(""));
        CHECK(!EnvironmentVariableManager::IsValidVariableName("   "));
        CHECK(!EnvironmentVariableManager::IsValidVariableName("BAD=NAME"));
        CHECK(EnvironmentVariableManager::IsValidVariableName("GOOD_VAR_123"));

        const DWORD pid = GetCurrentProcessId();
        const std::string varA = std::format("PSERV6_TEST_VAR_A_{}", pid);
        const std::string varB = std::format("PSERV6_TEST_VAR_B_{}", pid);
        const std::string varC = std::format("PSERV6_TEST_VAR_C_{}", pid);
        const std::string varALower = std::format("pserv6_test_var_a_{}", pid);

        struct Cleanup
        {
            std::vector<std::string> names;
            ~Cleanup()
            {
                for (const auto &n : names)
                {
                    EnvironmentVariableManager::DeleteEnvironmentVariable(n, EnvironmentVariableScope::User);
                }
            }
        } cleanup{{varA, varB, varC, varALower}};

        auto queryUserRegType = [](const std::string &name, std::string &outVal) -> DWORD
        {
            wil::unique_hkey hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
                return 0;
            std::wstring wName = utils::Utf8ToWide(name);
            DWORD type = 0;
            wchar_t buf[2048]{};
            DWORD bytes = sizeof(buf);
            if (RegQueryValueExW(hKey.get(), wName.c_str(), nullptr, &type, reinterpret_cast<BYTE *>(buf), &bytes) != ERROR_SUCCESS)
                return 0;
            outVal = utils::WideToUtf8(buf);
            return type;
        };

        // 1. Create REG_EXPAND_SZ variable and verify updating value preserves REG_EXPAND_SZ
        CHECK(EnvironmentVariableManager::SetEnvironmentVariable(varA, "%SystemRoot%\\Temp", EnvironmentVariableScope::User, REG_EXPAND_SZ));
        std::string readVal;
        CHECK(queryUserRegType(varA, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%SystemRoot%\\Temp");

        CHECK(EnvironmentVariableManager::SetEnvironmentVariable(varA, "%USERPROFILE%\\Data", EnvironmentVariableScope::User, 0));
        CHECK(queryUserRegType(varA, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%USERPROFILE%\\Data");

        // 2. Create a second variable to test collision rejection (including case-insensitive collision)
        CHECK(EnvironmentVariableManager::SetEnvironmentVariable(varB, "OriginalB", EnvironmentVariableScope::User, REG_SZ));
        CHECK(EnvironmentVariableManager::VariableExists(varB, EnvironmentVariableScope::User));
        CHECK(!EnvironmentVariableManager::RenameEnvironmentVariable(varA, varB, "ClobberAttempt", EnvironmentVariableScope::User, REG_EXPAND_SZ));
        // Neither varA nor varB may be lost or overwritten!
        CHECK(queryUserRegType(varA, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%USERPROFILE%\\Data");
        CHECK(queryUserRegType(varB, readVal) == REG_SZ);
        CHECK(readVal == "OriginalB");

        // 3. Safe rename to unused name preserves REG_EXPAND_SZ and removes old entry
        CHECK(EnvironmentVariableManager::RenameEnvironmentVariable(varA, varC, "%SystemRoot%\\Logs", EnvironmentVariableScope::User, REG_EXPAND_SZ));
        CHECK(!EnvironmentVariableManager::VariableExists(varA, EnvironmentVariableScope::User));
        CHECK(queryUserRegType(varC, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%SystemRoot%\\Logs");

        // 4. Case-only rename preserves REG_EXPAND_SZ and updates value
        const std::string varCLower = utils::ToLower(varC);
        CHECK(EnvironmentVariableManager::RenameEnvironmentVariable(varC, varCLower, "%SystemRoot%\\CaseRenamed", EnvironmentVariableScope::User, REG_EXPAND_SZ));
        CHECK(queryUserRegType(varCLower, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%SystemRoot%\\CaseRenamed");

        // 5. Controller rejects invalid names in SetPropertyEdit
        EnvironmentVariablesDataController envCtrl;
        DataController &envDc = envCtrl;
        EnvironmentVariableInfo info(varCLower, "%SystemRoot%\\CaseRenamed", EnvironmentVariableScope::User, REG_EXPAND_SZ);
        envDc.BeginPropertyEdits(&info);
        CHECK(!envDc.SetPropertyEdit(&info, 0, ""));
        CHECK(!envDc.SetPropertyEdit(&info, 0, "BAD=NAME"));
        CHECK(envDc.SetPropertyEdit(&info, 1, "%SystemRoot%\\CtrlUpdated"));
        CHECK(envDc.CommitPropertyEdits(&info));
        CHECK(queryUserRegType(varCLower, readVal) == REG_EXPAND_SZ);
        CHECK(readVal == "%SystemRoot%\\CtrlUpdated");
    }

    void TestContainerLifecycleAndCompaction()
    {
        std::cout << "[Test] DataObjectContainer refcounting, FinishRefresh compaction & move...\n";
        int liveCount = 0;
        {
            DataObjectContainer c;
            c.StartRefresh();
            auto *a = c.Append(new DummyObject("a", liveCount));
            auto *b = c.Append(new DummyObject("b", liveCount));
            auto *dup = c.Append(new DummyObject("a", liveCount));
            CHECK(dup == a);
            CHECK(liveCount == 2);
            CHECK(c.GetSize() == 2);
            c.FinishRefresh();
            CHECK(c.GetSize() == 2);

            // Second refresh sees only "b"; "a" must be released and compacted out.
            c.StartRefresh();
            CHECK(c.GetByStableId<DummyObject>("b") == b);
            c.FinishRefresh();
            CHECK(c.GetSize() == 1);
            CHECK(liveCount == 1);
            CHECK(c.GetByStableId<DummyObject>("a") == nullptr);

            DataObjectContainer moved = std::move(c);
            CHECK(moved.GetSize() == 1);
            CHECK(c.GetSize() == 0);
            CHECK(liveCount == 1);
        }
        CHECK(liveCount == 0);
    }

    void TestBase16ThemesAndAtomicToml()
    {
        std::cout << "[Test] Base16 theme loading & atomic TomlBackend persistence...\n";
        theme::ThemeSet ts;
        ts.LoadFromDirectory(L"pserv6\\themes");
        CHECK(ts.Themes().size() >= 20);
        int darkCount = 0;
        int lightCount = 0;
        for (const auto &info : ts.Themes())
        {
            if (info.dark) ++darkCount;
            else ++lightCount;
        }
        CHECK(darkCount > 0);
        CHECK(lightCount > 0);
        CHECK(ts.SelectByName("Tomorrow Night"));

        std::filesystem::path tempFile = std::filesystem::temp_directory_path() / std::format("pserv6_test_{}.toml", GetCurrentProcessId());
        std::filesystem::path tempArtifact = tempFile;
        tempArtifact += std::format(".{}.tmp", GetCurrentProcessId());
        std::error_code ec;
        std::filesystem::remove(tempFile, ec);
        {
            config::TomlBackend backend(tempFile.string());
            CHECK(backend.save("Application/Theme", std::string("Nord")));
            CHECK(backend.save("Application/FontFamily", std::string("Consolas")));
            CHECK(backend.save("Window/Width", static_cast<int32_t>(1440)));
        }
        CHECK(std::filesystem::exists(tempFile));
        CHECK(!std::filesystem::exists(tempArtifact));
        {
            config::TomlBackend reload(tempFile.string());
            std::string themeVal, fontVal;
            int32_t widthVal = 0;
            CHECK(reload.load("Application/Theme", themeVal) && themeVal == "Nord");
            CHECK(reload.load("Application/FontFamily", fontVal) && fontVal == "Consolas");
            CHECK(reload.load("Window/Width", widthVal) && widthVal == 1440);
        }
        std::filesystem::remove(tempFile, ec);
    }
} // namespace

int main()
{
    TestServicePropertyEditAndRemoteActions();
    TestEnvironmentVariableSafety();
    TestContainerLifecycleAndCompaction();
    TestBase16ThemesAndAtomicToml();

    if (g_failures > 0)
    {
        std::cerr << "\nFAILED: " << g_failures << " assertion(s) failed.\n";
        return 1;
    }
    std::cout << "\nALL TESTS PASSED.\n";
    return 0;
}
