# pserv6 Immediate Next Steps

This note turns the architecture direction into the next concrete work items.

## 1. Shell Bootstrap

Create a new WinUI application under `pserv6/` using:

- Windows App SDK
- C++/WinRT
- CMake

Use `C:\Projects\environ` as the technical reference for:

- CMake package bootstrap
- `cppwinrt.exe` generation
- self-contained Windows App SDK deployment
- manifest generation and runtime payload copying

## 2. Explorer Shell

Build one generic explorer shell before porting multiple views.

Required UI responsibilities:

- navigation
- command surface
- list/grid result view
- details/properties surface
- progress and error presentation

Do not build a services-only UI first. Build a generic explorer page that can host services as the first adapter.

## 3. Services Adapter

Use `Services` as the first vertical slice.

The new code contracts introduced in `pserv6/interaction/` are the starting point for that adapter.

The adapter must prove:

- controller-backed data refresh
- column description mapping
- selection by stable ID
- action discovery and execution
- editable details
- remote machine context

## 4. Refactoring Rule

Only refactor `pserv5` when the adapter or shell is blocked by an ImGui-era assumption.

If a cleanup does not unblock the `Services` slice, defer it.

## 5. First Real Deliverable

The first meaningful milestone is not "WinUI app starts".

The first meaningful milestone is:

- `pserv6` launches
- `Services` renders in the new explorer shell
- one non-destructive action works
- properties/details for one service can be displayed

