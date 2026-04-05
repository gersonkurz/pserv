# pserv6

`pserv6` is the next-generation UI shell for `pserv`.

Current direction:

- Windows App SDK
- WinUI 3
- C++/WinRT
- CMake

The existing `pserv5` codebase remains the source of truth for core system logic.
This subtree is for the new shell and the interaction contracts that sit between
the old core and the new UI.

Near-term goals:

- define the explorer/action/details contracts in plain C++
- bootstrap a WinUI application shell
- adapt `Services` as the first vertical slice

Current local build command:

```powershell
cmake -S pserv6 -B pserv6\build -G "Visual Studio 18 2026" -A x64
cmake --build pserv6\build --config Debug
```

Non-goals:

- cloning the ImGui UI one-for-one
- mass refactoring `pserv5` before the new shell exists
