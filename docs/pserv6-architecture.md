# pserv6 Architecture Note

## Goal

`pserv6` is a new Windows UI shell for `pserv`, built with:

- Windows App SDK
- WinUI 3
- C++/WinRT
- CMake

The goal is **not** to rewrite the existing business logic blindly. The goal is to keep the proven system-management core where it is still useful, replace the ImGui front end, and introduce a cleaner interaction layer between them.

This is a replatforming effort, not a cosmetic refresh.

## Non-Goals

- Do not refactor `pserv5` broadly for style or purity.
- Do not port all views at once.
- Do not redesign every manager/controller before a WinUI shell exists.
- Do not treat `pserv6` as a direct one-for-one clone of the ImGui UI.

## Working Assumption

The current codebase already has a useful split:

- `windows_api/`, `models/`, and much of `controllers/` contain real domain logic.
- `main_window.cpp` contains most of the current UI orchestration and policy.

That means the migration target is not "keep all of `pserv5` except widgets". The actual target is:

- keep resource enumeration and actions where they already work,
- extract or adapt the interaction contracts that are currently entangled with the ImGui shell,
- replace the UI shell entirely.

## Proposed Layers

`pserv6` should be treated as three layers.

### 1. Resource Backend

This layer contains the existing system-facing logic:

- Windows API wrappers
- resource models
- controller refresh logic
- action implementations, where they are not UI-specific

This layer should remain plain C++.

Examples from `pserv5`:

- `windows_api/service_manager.*`
- `models/service_info.*`
- `controllers/services_data_controller.*`

This layer is allowed to stay imperfect initially. The migration should only force refactoring where the current interfaces are too GUI-specific.

### 2. Interaction Layer

This is the new layer that `pserv5` mostly lacks as an explicit concept.

Its job is to describe how a resource view behaves without committing to ImGui or WinUI.

Responsibilities:

- selection model
- sorting and filtering state
- command/action discovery
- action execution and progress reporting
- editable field metadata
- details/properties presentation model
- error reporting contract
- navigation/view state
- remote context state where applicable

This layer should be UI-agnostic and should convert controller/action concepts into something the WinUI shell can bind to.

This is the layer that turns `pserv` into something closer to the old MMC idea:

- a generic explorer shell
- a result surface
- verbs/actions
- details/properties
- reusable interaction contracts

### 3. WinUI Shell

This layer is responsible for:

- application startup
- navigation shell
- page composition
- list/grid presentation
- command bars and context menus
- dialogs, flyouts, and details panes
- async operation visuals
- settings persistence integration

This layer should know nothing about Win32 service APIs, process enumeration, or registry internals. It should talk to the interaction layer.

## What Stays in pserv5 Core, Initially

The following should be reused early, with minimal change where possible:

- `DataObject`
- much of `DataObjectContainer`
- controller refresh logic
- manager classes in `windows_api/`
- most model classes
- existing action implementations that perform system work
- exporter logic
- CLI application

These are not sacred, but they are already useful assets.

## What Should Not Be Carried Over As-Is

The following are `pserv5` UI-era constructs and should be treated skeptically:

- `MainWindow` as a source of architecture
- direct dialog spawning from controllers
- direct `HWND`-centric assumptions in action dispatch
- selection lifetime coupled to the UI window
- ad hoc refresh policy living in the shell
- "properties dialog" as the only editing model

These are implementation artifacts of the ImGui app, not stable architectural truths.

## First Extraction Targets

Before or during the first `pserv6` vertical slice, extract these seams:

### Action Execution Service

Current problem:

- actions consume `DataActionDispatchContext`
- the context mixes UI handles, selection, async operation ownership, and post-action flags

Target:

- a UI-agnostic action execution service
- explicit request/response model for progress, confirmation, refresh, and errors

The WinUI shell can then provide:

- confirmation UI
- progress dialogs or inline progress
- error presentation

without actions knowing anything about the shell.

### View Descriptor / Explorer Adapter

Current problem:

- each controller exposes columns and actions, but the shell still owns a lot of view behavior

Target:

- an adapter that exposes a generic explorer-facing view model:
  - title
  - columns
  - items
  - selection mode
  - available actions
  - sortable/filterable fields
  - editable fields
  - details payload

This becomes the contract between old controllers and the new WinUI shell.

### Selection Model

Current problem:

- object selection ownership and cleanup are tied to the ImGui window code

Target:

- a reusable selection model independent of the shell
- stable identity based on resource IDs, not transient widget state

### Details / Properties Model

Current problem:

- "show properties dialog" is procedural and window-driven

Target:

- details described as structured editable/read-only fields
- shell decides whether to render them as:
  - side pane
  - modal dialog
  - separate page

## Proposed pserv6 Shell

Initial shell structure:

- `NavigationView` or equivalent shell navigation
- one generic explorer page
- top command surface
- central list/grid surface
- right-side details pane or modal details view
- global progress/error presenter

The shell should be generic enough that multiple resource views can plug into it with configuration rather than duplicated page logic.

## First Vertical Slice

The first slice should be `Services`.

Reason:

- it has list data
- it has sorting/filtering
- it has stateful actions
- it has long-running operations
- it has editable properties
- it has remote-machine context
- it exercises the most architectural seams early

If `Services` works well in the new shell, the architecture is probably viable. If it does not, that will surface the wrong assumptions before the rest of the app is ported.

## Recommended Migration Order

1. Create `pserv6` as a new CMake-based WinUI app.
2. Reuse the Windows App SDK bootstrap and self-contained deployment approach proven in `environ`.
3. Build the shell skeleton:
   - startup
   - navigation
   - generic explorer page
   - command surface
   - progress/error plumbing
4. Implement the interaction layer for one view.
5. Adapt `Services` into the new shell.
6. Only then decide what parts of `pserv5` need deeper refactoring.

## Design Constraint

Do not chase theoretical cleanliness up front.

Every abstraction introduced for `pserv6` must justify itself by surviving the `Services` slice. If it does not hold up there, it is probably architecture cosplay.

## Immediate Next Step

Define the first adapter contract in code:

- explorer view descriptor
- item identity and selection model
- action execution contract
- details/properties contract

That contract should be small, explicit, and good enough to host `Services` without dragging ImGui-era assumptions into the new shell.
