# pserv6 / `pnq::ui` — migration plan

**Status:** revision 3. Plan approved by review 2026-09-18; O2, O3 and O7 closed by the
owner as D7, D8, D9. Nothing implemented.
**Date:** 2026-09-18
**Scope:** three repositories — `pnq`, `environ`, `pserv`.

---

## 1. Goal

Replace pserv5's Dear ImGui frontend with a Direct2D/DirectWrite one, by extracting
environ's UI host into a shared library that **both** environ and pserv6 consume. The
library is the deliverable, not a side effect: two consumers is what stops it from being
"environ's UI with a second caller".

Two things come with it, and neither is free:

- pserv5 carries a namespace-renamed fork of pnq, which the port can retire.
- pserv5's "shared core" is shared between the CLI and *the ImGui GUI specifically*. A
  second GUI does not fit through that seam as it stands. See 2.5.

Success, in order:

1. `pnq::ui` exists; environ is built on it; environ is behaviourally unchanged.
2. pserv's core is frontend-agnostic for real, not just for the CLI.
3. pserv6 renders all ten views on `pnq::ui` at parity with pserv5.
4. pserv5's forked config / refcount / logging / string code is gone, replaced by pnq,
   with settings and log behaviour either preserved or deliberately migrated.

---

## 2. Where we actually are

Measured 2026-09-18. Line counts are `wc -l` over `.cpp`/`.h`.

### 2.1 pserv5 — 20,219 LOC

| Layer | LOC | Fate |
|---|---:|---|
| `main_window.cpp` + `.h` | 2,510 | replaced |
| `dialogs/` | 746 | replaced |
| `theme/` | 370 | replaced by `pnq::ui` theme |
| `core/`, `models/`, `controllers/`, `actions/`, `windows_api/`, `core/exporters/`, `pservc/` | ~16,600 | mostly transfers — **but see 2.5** |

Eleven files reference ImGui directly. That number is misleading on its own, because the
GUI/CLI seam is wider than those eleven files (2.5).

Forked from pnq, and retired in Phase 8:

| pserv5 | pnq equivalent | LOC |
|---|---|---:|
| `config/` — same classes, but **not** a pure rename: pserv has an out-of-line `section.cpp` where pnq's `Section` is inline, and `typed_value.h:50` carries construction-time logging pnq's has none of | `pnq/config/` (`pnq::config`, header-only) | ~1,075 |
| `core/refcount_interface.h` | `pnq/ref_counted.h` (superset; macro block verbatim) | 121 |
| `utils/logging.*` | `pnq/logging.h`, `pnq/log.h` (**different behaviour** — see O6) | 228 |
| `utils/win32_error.h` | `pnq/windows_errors.h` | 129 |
| `utils/string_utils.h` | `pnq/string.h`, `pnq/unicode.h`, `pnq/win32/wstr_param.h` | 63 |
| `utils/base_app.h` | `pnq/app_init.h` (**different behaviour** — see O6) | 85 |
| `windows_api/service_manager.*` | `pnq/win32/service.h` (overlap, not identical) | — |

environ already consumes the real thing (`AppSettings : public Section`, with
`using pnq::config::Section`). pserv5 is the odd one out.

### 2.2 environ — 9,041 LOC (`src/core` 1,988, `src/win32` 7,053)

| File(s) | LOC | Fate |
|---|---:|---|
| `theme.*` | 362 | extract, mechanical — **must move in 2a**: `d2dwindow.h:3` and `view.h:10` both include it, and the theme picker calls `ThemeSet` methods implemented in `theme.cpp`. Merging pserv5's second implementation happens later, in 2b. |
| `d2dwindow.*` | 503 | extract, mechanical |
| `view.h` | 81 | extract, mechanical — the best idea in the codebase |
| `themeselectionview.*` | 434 | extract, mechanical |
| `fontselectionview.*` | 428 | extract, mechanical |
| `app.*` | 96 | **split, not moved** — it defines `wWinMain`, constructs environ's concrete `MainWindow`, and hard-codes environ's logging identity. Only the DPI/COM/message-loop shell is library material; the entry point stays application-owned. |
| `grid.*` | 1,800 | **rewrite** (see 2.4) |
| `gridview.*` | 1,041 | stays in environ (env-domain) |
| `mainwindow.*` | 1,721 | stays; loses its hand-rolled modal/nav code to the library |
| `historyview.*` | 587 | stays (env-domain) |

Mechanically extractable as one dependency-complete unit: **1,808 LOC**. The adaptation work
that follows it (theme-loader merge, renderer activation) changes those files rather than
adding new ones.

### 2.3 pnq — 48 headers, 10,623 LOC

Header-only C++23, MIT. CMake with `pnq::pnq` as an INTERFACE target, Catch2 v3 via
FetchContent, ctest on x64 **and** ARM64, `/W4 /WX` — warnings are build failures.
**The only repo in this family with a test suite and a real quality gate.**

Two documented invariants constrain what it may become:

- "no .cpp files except the tests";
- a cross-platform split with a macOS target (`docs/cross-platform-plan.md`; regis3 is
  deliberately halved into a portable core and a `PNQ_PLATFORM_WINDOWS` layer).

### 2.4 The grid gap

ImGui surface currently in use by pserv5, mapped to required `pnq::ui` components:

| ImGui in use | `pnq::ui` need | Exists in environ? |
|---|---|---|
| `BeginTable` + `Resizable`, `Reorderable`, `Hideable`, `Sortable`, `ScrollX`, `ScrollY`, `RowBg`, `Borders`, `SizingFixedFit`, `TableGetSortSpecs`, `TableSetupScrollFreeze` | N-column virtualized grid, per-column resize/reorder/hide, typed sort, persisted layout, frozen header | **No** — environ's grid is 2 fixed columns |
| `BeginMenuBar`, `BeginMenu`, `MenuItem` | menu bar with submenus and checkable items | **No** — 9 flat hand-hit-tested nav items |
| `BeginPopupContextItem` | context menu built per selection from `GetActions()` | **No** |
| `BeginTabBar`, `BeginTabItem` | view switcher across 10 views | **No** |
| `InputText`, `InputTextMultiline`, `InputInt`, `BeginCombo`, `Button`, `ProgressBar`, `CollapsingHeader` | form primitives for the properties dialog | **Partly** — environ has one inline `EDIT` and hand-painted buttons |
| `BeginPopupModal` | modal card + scrim | **Yes** — environ's MsgBox/review overlay, needs generalising |
| `Image` | bitmap draw (splash) | No (trivial) |
| `Selectable`, multi-select, filter, `SetScrollHereY` | in the grid | Yes, portable |

environ's `Grid` cannot be "converted". Its API is env-domain: `SetData()` takes three
`vector<EnvVariable>` plus an `elevated` flag; rows carry `Scope`/`varIndex`/`segIndex`;
`RowGroup` is `{User, Machine, Process, Inherited}`; columns are literally `col1`/`col2`;
sort is `enum SortColumn {Name, Value}`; and much of the public surface is
`SelectionShadowed()`, `SelectedIsPathEntry()`, `CurrentVars(scope)`. What survives is the
*mechanism* — virtualization, scrollbar thumb drag, horizontal scroll extent measuring,
hover/selection state, hosting a real `EDIT` for inline editing. The body is new code
against an abstract row/column source shaped by pserv's existing `DataObjectColumn`.

pserv's `DataController` exposes columns with `ColumnDataType` / `ColumnAlignment` /
`ColumnEditType`, plus `GetVisualState()` and `GetActions()`. That is the grid's
data-source interface, already written and already exercised by `pservc`.

### 2.5 The frontend seam is not where the docs say it is

`AGENTS.md` says GUI-only code is excluded from the CLI with `PSERV_CONSOLE_BUILD`. True —
but that macro separates the CLI from **the ImGui GUI**, not from "a GUI". It appears in
**49 files**, and those blocks are of two kinds that must be **classified, not assumed**:

- **Frontend-neutral Win32**, which pserv6 can keep as-is. Example:
  `actions/common_actions.cpp:171` guards a plain `MessageBoxA` — usable by either GUI.
- **ImGui-specific**, which pserv6 cannot use at all. The table below is what the audit has
  found so far; Phase 4 starts by classifying all 49.

A second GUI does not fit through the seam until that classification is done and the second
kind is relocated.

Concretely, in code the earlier revision of this plan listed as "transfers unchanged":

| Location | Coupling |
|---|---|
| `core/data_controller.h/.cpp` | The base controller **owns a GUI dialog**: `DataPropertiesDialog *m_pPropertiesDialog`, `ShowPropertiesDialog()`, `RenderPropertiesDialog()`, `HasPropertiesDialogWithEdits()`. `data_controller.cpp:30` constructs the ImGui dialog directly. |
| `controllers/environment_variables_data_controller.cpp:169–` | A complete ImGui modal ("Add Environment Variable") inside a controller. |
| `utils/string_utils.h:59` | `CopyToClipboard` calls `ImGui::SetClipboardText` in GUI builds. Sole clipboard path. |
| `precomp.h:72–75` | Pulls `imgui.h`, `imgui_impl_win32.h`, `imgui_impl_dx11.h`, `imgui_internal.h` into the GUI PCH. |
| `core/data_action.h`, `core/data_action_dispatch_context.h` | Dispatch context carries GUI handles across the seam. |

So the port has a prerequisite the earlier revision missed: **make the core actually
frontend-agnostic** (Phase 4), before pserv6 integration and before Phase 9 can delete
ImGui. This also means the old non-goal "no controller changes for UI reasons" was wrong;
it is restated in section 10.

---

## 3. Decisions taken

| # | Decision | Rationale |
|---|---|---|
| **D1** | **No Win32 menus.** The menu is drawn by `pnq::ui`. | environ's look is the point of the exercise; native menus cannot follow a Base16 scheme. The cost is accepted and split into "must implement" vs "accepted limitation" in R2. |
| **D2** | **environ is refactored first.** Both apps then prove the library. | A library shaped by one consumer is not a library. environ is the smaller app and can absorb churn. |
| **D3** | **pnq gains a second target; no third repo.** `pnq::pnq` (INTERFACE, portable) + `pnq::ui` (STATIC, `if(WIN32)`, depends on `pnq::pnq`). | The UI library needs pnq. A separate repo means nested submodules in both consumer apps and permanent version skew between two of the author's own libraries. Cost: pnq's "header-only" invariant acquires a documented exception. |
| **D4** | **pserv6 is a branch on `pserv`, and becomes main at parity.** `pserv6.exe` builds alongside `pserv5.exe` and `pservc.exe` on that branch. | Side-by-side comparison of old and new frontend against one shared core. A separate repo forks the core. |
| **D5** | **The grid is a rewrite, not a port.** | See 2.4. Calling it a port makes the schedule lie. |
| **D6** | **The core is made frontend-agnostic before pserv6 integration**, as its own phase with its own review. | See 2.5. Doing it inside the pserv6 work would entangle a core refactor with a UI rewrite, and neither could be reviewed on its own merits. |
| **D7** | **Migrate to D2D 1.1**: a device context on a flip-model DXGI swapchain, replacing environ's `CreateHwndRenderTarget`. *(closed 2026-09-18; was O2)* | pserv5 already runs DX11 + swapchain, and pserv6 has ten views with 2-second auto-refresh over thousands of rows. Far cheaper now than once two apps depend on the paint path. Sequenced as **implement** (Phase 1) → **move** (Phase 2a, existing path retained) → **activate** (Phase 2b), so the extraction and the renderer swap never land together. |
| **D8** | **`pnq::ui` carries its own `.vcxproj` beside its `CMakeLists.txt`**; both app solutions consume it as a project reference. *(closed 2026-09-18; was O3)* | The alternative — listing every library `.cpp` in each consumer — costs five edits per new file and grows with each consumer, against `pserv5.vcxproj`'s 62 and `pservc.vcxproj`'s 53 hand-maintained entries plus three `.filters` files (`AGENTS.md`: "not discovered automatically"). A project reference costs three edits once and gives real incremental builds. CMake stays authoritative for pnq's own build and tests. **The residual cost is a configuration contract, not just a shared file list** — see D8a. |
| **D8a** | **The `.vcxproj` and the `CMakeLists.txt` must agree on a stated configuration contract, and consumers must agree with it too.** | Source membership is not the whole interface, and a mismatch does **not** reliably fail at link time — an unreferenced missing translation unit links fine, and a definition mismatch can violate the ODR and may link without a diagnostic. The configuration difference below establishes the risk; no mixed-configuration executable has been built to characterise the outcome, and none should be assumed. Concretely, `pnq/CMakeLists.txt` supplies `cxx_std_23`, `UNICODE`/`_UNICODE`, `/W4 /WX` and the `PNQ_USE_QUILL` backend switch; **both apps additionally define `SPDLOG_USE_STD_FORMAT`, which pnq's CMake does not** — and that macro changes spdlog's types across the boundary. The contract must enumerate: language standard, compile definitions (including the spdlog/Quill selection), runtime library, character set, warning level, and the per-config/per-arch output layout. It is written down in Phase 1 and checked by Phase 1's consumer smoke test. |
| **D9** | **x86 is retired.** pserv6 ships x64 and ARM64 only. *(closed 2026-09-18; was O7)* | Owner decision. Retired explicitly and early rather than left to rot through a test matrix that never covered it. Phase 0 removes it from the solution, the justfile and packaging so no later phase spends time building it. |

---

## 4. Open decisions

Each blocks a specific phase. None should be settled by drift.

| # | Question | Blocks | Recommendation |
|---|---|---|---|
| **O1** | Grid's first consumer: pserv6's flat N-column case, or environ's tree-with-staging case? | Phase 6 | Build for pserv6 first; migrate environ's grid afterwards. Swallowing both from day one buys a tree model *and* a staging model the library may never otherwise need. Phase 5 exists so this consumer is real before the grid is written. |
| **O4** | Do the three repos unify their vendored `spdlog` / `tomlplusplus` / `fkYAML` revisions? | Phase 0 | Yes for `fkYAML` — theme loading moves into the library, so one version is forced. The others: pin when they collide, not before. |
| **O5** | Does pserv6 keep the splash screen, remote-machine support, and the full ImGui-era properties dialog feature set? | Phase 7 | Enumerate before Phase 5. Parity is defined by a written list, not by memory. |
| **O6** | **Settings and log compatibility.** pserv writes to `FOLDERID_LocalAppData` (`utils/logging.cpp:63`); `pnq::app_init` uses `path::get_roaming_app_data` (`app_init.h:49`). pserv's bootstrap also applies a configured log path and level; pnq's does not. Preserve pserv's behaviour, migrate existing users' files, or change location deliberately? | Phase 8 | Preserve location by passing pserv's path in rather than adopting pnq's default, unless the owner wants the move. Either way an existing 5.x config must load, save and reload unchanged — as an **executed** check, all three executables. |

---

## 5. `pnq::ui` component inventory

| Component | Origin | Effort |
|---|---|---|
| `D2DWindow` host — window, D2D lifecycle, `TextFormat` RAII, custom caption, DPI | environ, mechanical | extract |
| `View` / `ViewContext` | environ, mechanical | extract |
| Theme picker, font picker | environ, mechanical | extract |
| App shell — DPI awareness, COM init, message loop (**not** `wWinMain`, **not** the concrete window, **not** logging identity) | environ `app.cpp`, split | small |
| `theme` — Base16 YAML → `ColorScheme` | environ, mechanical (2a — `d2dwindow`/`view.h`/pickers depend on it); pserv5's second implementation merged in (2b) | extract, then adapt |
| Modal card + scrim + buttons | environ `mainwindow.cpp`, generalise | small |
| **Menu**: bar, submenus, checkable items, context menus, keyboard navigation (arrows, mnemonics, Esc), data-driven item model | **new** | large |
| **Form primitives**: text, multiline, integer, combo, tabs, progress | mostly **new** (environ has one inline `EDIT`) | medium |
| **Grid**: N columns, resize/reorder/hide, persisted layout, typed sort, multi-select, filter, inline edit, per-row visual state | **rewrite**, mechanism from environ | large |
| View switcher (10 views) | **new** | small |
| Bitmap draw | **new** | trivial |

---

## 6. Phases

A phase is done when its exit criteria are met and the yaaadabi review for the affected
repo has approved it. Each phase names what must be **executed**, not merely read.

### Phase 0 — groundwork

- Fill the **Loop parameters** block in all three repos. All three are unusable today: pnq
  has no yaaadabi wiring; environ's and pserv's are the stock placeholder.
- Add `pnq` as a submodule to `pserv`.
- Decide **O4**.
- **Retire x86 (D9)**, so no later phase builds an architecture that is being dropped:
  remove the `x86` platform from `pserv5.slnx`, the two `x86` pairs from `just build-all`,
  the `package-x86` recipe and its use in `package-all`, `setup/pserv5-x86.msis`, and the
  `source_32bit` entry in `setup/setup-bundle.msis`.
- Exit: three repos have a filled Loop block; builds green everywhere on their full
  architecture matrices.

### Phase 1 — `pnq::ui` skeleton

- `pnq::ui` STATIC target in pnq, `if(WIN32)`, headers under `include/pnq/ui/`, `/W4 /WX`,
  wired into the existing Catch2 suite.
- D7's D2D 1.1 backend (device context on a flip-model DXGI swapchain) is **implemented**
  here, once — written, and not yet the active path for any app. Activation is Phase 2b.
- Ship D8's `.vcxproj` beside the `CMakeLists.txt`, and write down D8a's configuration
  contract next to them.
- Exit, and a library that merely *builds* does not satisfy this:
  - builds and tests on x64 + ARM64 under CMake;
  - a **throwaway executable** consumes it — includes a public header, calls an out-of-line
    library API, links through `ProjectReference`, and runs a smoke check that executes;
  - that consumer is built and **run** across Debug/Release × x64/ARM64, with a record of
    which binaries actually ran;
  - the consumer's own settings match D8a's contract, proving the contract can be written
    down and followed before either real app depends on it.

### Phase 2a — mechanical extraction

- Move `theme.*`, `d2dwindow`, `view.h`, both pickers, and the app-shell split of `app.cpp`
  into `pnq::ui` (1,808 LOC). **Pure move**: no behaviour change, no refactor, no merge.
  environ keeps its own `wWinMain`, its own `MainWindow`, its own logging identity.
- `theme.*` is in this set because it is a **dependency of the set**, not an afterthought:
  `d2dwindow.h:3` and `view.h:10` include it, and the theme picker calls `ThemeSet` methods
  living in `theme.cpp`. Without it the extracted library would still depend on
  application-owned code, which breaks both the upstream-first rule and D8's premise that
  the library builds on its own as a referenced project.
- **The existing rendering path is retained unchanged.** Phase 1's new backend exists but is
  not switched on here; that is what keeps this phase a pure move.
- Exit: environ builds warning-free and behaves identically. Executed checks, not
  screenshots alone: launch, resize, DPI change across monitors, theme switch, font picker
  open/preview/cancel/accept, caption buttons, grid scroll and inline edit. Record the list
  and its result.

### Phase 2b — adaptation

- Merge pserv5's Base16 loader into the library's `theme.*` (already moved in 2a).
- **Activate** D7's renderer under environ — the switch, not the implementation.
- These *are* behaviour changes, which is why they are not in 2a. Splitting implement (1)
  from move (2a) from activate (2b) means a regression has exactly one candidate cause.
- Exit: same executed check list as 2a, re-run; theme loading has headless tests in pnq;
  every theme in `themes/` renders.

### Phase 3 — menu and forms

- Build the menu component and form primitives against environ, improving environ's own
  menu as the first customer (D1's payoff).
- Exit: environ's nav is the library's menu; keyboard navigation executed and recorded;
  menu item model and hit-testing have headless tests; `/W4 /WX` clean.
- **Prerequisite of:** Phase 5 (shell menu) and Phase 7 (forms).

### Phase 4 — pserv frontend seam

Independent of the library; can run in parallel with Phases 1–3. On the `pserv6` branch.

- **Classify all 49 `PSERV_CONSOLE_BUILD` sites first** (see 2.5): frontend-neutral Win32
  stays; ImGui-specific gets relocated. The classification is the phase's first deliverable,
  because it sizes the rest of it.
- Move the properties-dialog lifecycle out of `DataController` behind a frontend-agnostic
  interface the GUI implements.
- Move the ImGui modal out of `environment_variables_data_controller.cpp`.
- Give `CopyToClipboard` a Win32 implementation for GUI builds, so it stops depending on
  ImGui.
- Separate the ImGui PCH includes from the shared PCH.
- Audit `data_action.h` / `data_action_dispatch_context.h` for GUI handles crossing the seam.
- Exit: **pserv5 still builds and runs unchanged** on x64/ARM64 (x86 retired per D9),
  `pservc` unchanged,
  and the core compiles against a null frontend. Executed: pserv5 exercised across all ten
  views; `pservc` listing commands run.

### Phase 5 — minimal pserv6 host

- `pserv6.exe` target beside `pserv5.exe` and `pservc.exe`. Window, caption, theme, menu,
  view switcher. No views yet.
- Exists here, before the grid, so Phase 6 has a real first consumer.
- Exit: pserv6 starts, switches themes, opens menus. Parity list from O5 written down.

### Phase 6 — the grid

- New N-column grid per O1, consumed by the Phase 5 host.
- Headless tests in pnq: row building, sort across all `ColumnDataType`s, filter, layout
  maths, column persistence.
- Exit: grid tests green; pserv6 renders one real view's live data.

### Phase 7 — pserv6 views

- Ten views onto the new grid, driven by the now frontend-agnostic `DataController` API.
- Properties dialog, context menus from `GetActions()`, async operations, export.
- Exit: O5's written parity list fully ticked, **plus** executed acceptance checks for the
  behaviour `AGENTS.md` calls out and that no headless test can reach: selection survival
  across refresh and across object removal; action dispatch against a filtered subset;
  pending-edit handling during auto-refresh; async operation completion **and**
  cancellation with the dispatch context outliving a refresh; destructive-action
  confirmation. These live in pserv, not pnq — they are application checks and must not
  drag pserv dependencies into the library.

### Phase 8 — retire the fork

- pserv6 and `pservc` adopt `pnq::config`, `pnq::ref_counted`, `pnq::logging`,
  `pnq::windows_errors`, `pnq::string`/`unicode`/`wstr_param`, `pnq::app_init`.
- O6 settled and implemented.
- Exit: ~1,500+ LOC deleted. Executed: an existing 5.x config file loads, saves and reloads
  with no setting lost, for `pserv5.exe`, `pserv6.exe` and `pservc.exe`; log file lands
  where O6 says it should; configured log level and path still apply.

### Phase 9 — cutover

- Delete `pserv5.exe`, `main_window.*`, `dialogs/`, `theme/`, and the `imgui` submodule —
  now possible because Phase 4 removed ImGui from shared code.
- Update README / docs / `instructions.md` — the README still says "Visual Studio 2022" and
  `pserv5.sln` and omits fkYAML; `instructions.md` claims "100% complete".
- Merge `pserv6` to main. Version 6.0.0. MSIS scripts updated for the full arch matrix.
- Exit: one GUI, one CLI, no ImGui in the tree; `just release` produces all shipped
  architectures per D9 — x64 and ARM64, with the bundle no longer offering a 32-bit payload.

---

## 7. Priorities and parallelism

**Critical path:** 0 → 1 → 2a → 2b → 5 → 6 → 7 → 8 → 9.

**Parallel:** Phase 3 (menu/forms) runs alongside 2b–5; it is a prerequisite of Phase 5's
menu and Phase 7's forms, so it must land before those, not before the grid. Phase 4 (pserv
frontend seam) touches only the pserv repo and is independent of the library entirely — it
can start on day one and is a prerequisite of Phase 5 and of Phase 9's ImGui deletion.

**Priority order if effort is scarce:**

1. **Phase 2a.** Extraction with a proven-unchanged environ is the only phase that pays off
   even if everything after it is abandoned — the library exists and is proven.
2. **Phase 4.** Pure win regardless of the UI work: the core stops lying about being
   frontend-agnostic, and pserv5 keeps working throughout.
3. **Phase 6.** The grid is the project. Everything after it is assembly.
4. **Phase 8.** Deletion, independently valuable, but gated on O6 being answered.
5. **Phase 3.** Large, and only pserv6 truly needs the hierarchy; environ can limp on its
   existing nav.

**Stop-and-reassess line:** if Phase 6 exceeds its estimate by 2×, the fallback is pserv6
shipping with fewer views on the new grid while pserv5 stays for the rest. D4's
side-by-side build is what makes that survivable rather than fatal. Note the cost: Phase 9
cannot complete, so the ImGui submodule and the fork both survive until it does.

---

## 8. Workflow rules

**Upstream-first.** A change needed by both apps lands in pnq first; the apps follow. Never
fix in the app and backport — that is how the pserv5 config fork happened in the first place.

**Submodule pinning.** Apps pin pnq by SHA. Bumping it is its own commit, whose message
names what it picks up. No drive-by bumps inside feature commits.

**Branches.** pnq and environ work on `main`. pserv works on `pserv6` until Phase 9.

**Review loop.** The yaaadabi protocol applies per repo, so each repo needs its own filled
Loop parameters block (Phase 0). A change spanning two repos is two reviews, pnq's first.

**Architecture matrices**, which differ and must not be conflated:

- pnq: x64 + ARM64 (its existing ctest matrix).
- environ: x64 + ARM64 (its existing build matrix).
- **pserv: x64 + ARM64.** x86 retired per D9 in Phase 0; until that lands, `just build-all`
  and `just package-all` still carry it.

**Quality gate.** `/W4 /WX` everywhere, including pserv6 — pnq already enforces it, environ
declares it informally, pserv has no stated policy today.

**Where tests live.** Frontend-agnostic logic is tested headlessly in pnq's Catch2 suite:
grid row building / sort / filter / layout / column persistence; menu item model and
hit-testing; theme loading and Base16 mapping. **Application behaviour is checked in its own
repo** — pserv's selection/refresh/async/confirmation checks (Phase 7) and config
compatibility checks (Phase 8) must not push pserv dependencies into pnq. Paint is not
tested anywhere; that gap is named rather than discovered later.

**Definition of done** for any unit of work: compiles `/W4 /WX` on that repo's full
architecture matrix above; headless tests green where the code is testable; the affected
app **executed** and what was executed stated explicitly; reviewer approved.

**Doc consistency.** Each repo's `CLAUDE.md`/`AGENTS.md` owns its own build and layout. The
cross-repo picture lives only in this document, updated as decisions close. `AGENTS.md`'s
claim that `PSERV_CONSOLE_BUILD` separates GUI from CLI needs correcting in Phase 4 — it
separates the CLI from *the ImGui GUI*.

---

## 9. Risks

| # | Risk | Mitigation |
|---|---|---|
| **R1** | Grid scope explosion — the generic grid grows a tree model, a staging model and a plugin system before it renders a row. | O1 picks one consumer first, and Phase 5 makes that consumer real before the grid is written. Tests defined before features. What the first consumer does not need is not built. |
| **R2** | **Keyboard** and **accessibility** are two different exposures, and D1 forgoes native support for both. | **Must implement and verify:** full keyboard operation of menus, grid and forms (arrows, mnemonics, Tab, Esc), and real `EDIT` controls for text entry so IME and clipboard behave. **Accepted limitation, stated not hidden:** custom-drawn menus, grid and forms expose no UI Automation, so screen readers will not see them; pserv5's ImGui frontend does not either, so this is not a regression, but it is a ceiling. Revisit only if a user needs it. "Keyboard navigation works" must never be reported as accessibility done. |
| **R3** | environ regresses during extraction and nobody notices — it has no tests. | Phase 2a is a pure move with an executed check list (launch, resize, DPI change, theme switch, font preview/cancel/accept, caption, scroll, inline edit). Behaviour-changing work is quarantined in 2b and re-runs the same list. Screenshots supplement the list; they do not replace it. |
| **R4** | The CMake/MSBuild seam becomes a per-phase tax, **or drifts silently**. | D8 settles the mechanism once in Phase 1: one `.vcxproj` in the library, referenced by both solutions. Drift is the sharper risk and D8a is its mitigation: the two definitions must agree on a written configuration contract, not merely a file list, because a mismatch does **not** reliably fail at link time — `SPDLOG_USE_STD_FORMAT` differing across the boundary can violate the ODR and may link without a diagnostic. Phase 1's exit runs an actual consumer executable across Debug/Release × x64/ARM64 rather than only building the library. |
| **R5** | Two frontends in one branch doubles maintenance during Phases 5–8. | Fixes go to the shared core, which both use. No pserv5-only UI fixes on the branch — it is scheduled for deletion. Phase 4's exit criterion keeps pserv5 working, so it stays a usable fallback. |
| **R6** | The port rewrites everything driving the controllers, with no safety net. | Phase 4 lands first and keeps pserv5 runnable as the reference. `pservc` is the canary for core-side regressions. Phase 7's executed acceptance checks cover the GUI-only behaviour no headless test reaches. |
| **R7** | Phase 8 silently changes where users' settings and logs live. | O6 must be answered before Phase 8 starts, and the phase's exit requires an executed old-config load/save/reload across all three executables. |
| **R8** | pnq stops being a coherent thing. | `pnq::ui` is `if(WIN32)`, links nothing into `pnq::pnq`, and touches no portable header. A macOS consumer links `pnq::pnq` and never compiles a line of D2D. |

---

## 10. Non-goals

- Migrating environ or pserv to CMake.
- Any new pserv feature during the port. Parity first, features after.
- A general-purpose UI toolkit. `pnq::ui` serves two apps; anything with no caller is not built.
- Cross-platform UI. `pnq::ui` is Windows and Direct2D, and is expected to stay that way.
- **Not a non-goal, corrected from revision 1:** the core *will* be changed for frontend
  reasons — that is Phase 4, and it is a prerequisite, not a side effect. What remains out
  of scope is changing controller, action or `windows_api` **behaviour**: Phase 4 relocates
  the frontend seam and must not alter what any operation does.
