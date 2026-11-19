#pragma once
#include <cstdint>

namespace pserv
{
    enum class EnvironmentRead { Missing, Present, Failed };
    enum class EnvironmentEditResult { Success, ReadFailed, Collision, WriteFailed, DeleteFailed, RollbackFailed };

    // Backends supply read-type, write and erase operations. Never remove the
    // source until the replacement exists; never overwrite an existing target.
    template<typename Name, typename Value, typename Read, typename Write, typename Erase>
    EnvironmentEditResult EditEnvironmentValue(Name const& original, Name const& target, Value const& value,
        bool sameName, Read read, Write write, Erase erase)
    {
        uint32_t type = 1; // REG_SZ for a newly created variable.
        auto source = read(original, type);
        if (source == EnvironmentRead::Failed || (!sameName && source == EnvironmentRead::Missing))
            return EnvironmentEditResult::ReadFailed;
        if (!sameName)
        {
            uint32_t ignored{};
            auto destination = read(target, ignored);
            if (destination == EnvironmentRead::Failed) return EnvironmentEditResult::ReadFailed;
            if (destination == EnvironmentRead::Present) return EnvironmentEditResult::Collision;
        }
        if (!write(target, value, type)) return EnvironmentEditResult::WriteFailed;
        if (!sameName && !erase(original))
            return erase(target) ? EnvironmentEditResult::DeleteFailed : EnvironmentEditResult::RollbackFailed;
        return EnvironmentEditResult::Success;
    }
}
