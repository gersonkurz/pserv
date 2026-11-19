# Release review — 2026-11-18

## Changes from the two UI reviews

- Restored the original application icon and all 20 Base16 palette files. Themes now use a menu grouped by dark/light classification.
- Removed the duplicate provider heading and combined actions, elevation, refresh and search into one toolbar. Buttons are icons with tooltips and accessible names.
- Replaced the table-only font picker with the Windows font dialog, sample preview, live Apply and Cancel restoration. Family, weight, italic and size apply throughout the application.
- Removed whole-table state counting and repeated action reconstruction from selection changes. Plain mouse selection is updated on pointer press; Ctrl/Shift preserve native extended selection. Measured selection-handler work with refresh paused was 76–119 microseconds, not an end-to-end rendering benchmark.
- Preserved row identities, selection and scroll across refresh; fixed short-table alignment and the off-screen vertical scrollbar. Refresh time and duration are labelled separately.
- Added persistent per-view columns/order/widths, filter/sort/live state, configurable refresh intervals, window placement and font/zoom.
- Added local/remote menus, clipboard and file export, properties/editing, About/documentation and explicit elevation. Remote support is limited to the Services and Devices providers.
- Fixed service display-name edits and remote SCM routing. Fixed an environment-variable rename path that deleted the source before a potentially failing write; replacement now preserves source/type, rejects collisions and reports rollback failure. Fixed misleading startup-type completion text.
- Consolidated the solution under pserv6, renamed the CLI pserv6c, removed the old frontend/submodule, and integrated full self-contained runtime staging and the final bundle into package-all.

## Regression coverage

`just test` builds native Release and runs four focused checks:

- Service property mapping, identity/invalid-start-type rejection, failed configuration, successful model updates and remote-machine forwarding using a fake SCM boundary.
- Production environment-edit algorithm: success/type preservation, collision, read/write failure, delete failure with successful rollback, and rollback failure retaining the original.
- Row reconciliation: unchanged snapshots emit no collection events; insertion/removal/reordering preserve surviving object identity.
- Actual DataRow collection: XAML-required IVector/IInspectable and IIterable/IInspectable share the cell contents. This guards the earlier startup crash.

## Runtime checks and limits

Earlier ARM64 checks exercised all ten providers, including over 10,000 module rows; filtering, typed sorting, mixed selection/actions, scroll and selection retention, disabled-service colours, Properties without applying edits, clipboard plain text/JSON, and remote connection using this computer's name. The native font dialog now shows samples; Apply changes the surrounding interface and Cancel restores it.

Installer creation and payload inspection do not substitute for clean-machine install/upgrade/uninstall testing. A separate remote host, UAC elevation round-trip and live service/process/task/uninstaller mutations require a disposable test environment. No such mutations are needed to verify layout or packaging, and they have not been exercised on this working machine. These are release-validation gaps, not evidence that all administrative workflows are correct.
`pserv6c` export review also fixed progress/banner text on stdout, signed-byte JSON escaping of UTF-8, and codepage conversion of redirected output. JSON/CSV now use UTF-8 on redirected stdout, with diagnostics on stderr.

## Final verification

Release GUI and CLI compile for ARM64, x64 and x86; ARM64 Debug also compiles. All four regression checks pass. Native ARM64 JSON and CSV parse directly and preserve the EventLog service's Unicode description; the JSON reference was checked against Windows WMI. Column hiding/reordering survives restart, and Shift-selection remains functional with refresh paused. All 14 dark and 6 light Base16 palettes appear in the grouped menus.

The staged ARM64 application starts and enumerates services. Its loaded MSVCP140.dll, VCRUNTIME140.dll and Microsoft.UI.Xaml.dll come from the staged directory. Release builds copy the matching v145 CRT from the installed VS toolchain; this avoids depending on MSIS's older predefined redistributable. The [Microsoft runtime guidance](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) requires a runtime at least as recent as the compiler.

`just package-all` creates the three MSIs and final bundle, then runs `scripts/verify-release.ps1`. It checks executable architecture, core runtime files, manuals and every theme against MSI file metadata, and checks that the bundle embeds the current MSIs by digest. This is artifact verification; the clean-machine installation and live administrative workflow gaps above remain. Existing provider warnings and WiX's automatic ARM64 InstallerVersion adjustment remain build warnings.
