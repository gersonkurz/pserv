# Settings and logs

[Back to the contents](index.md)

pserv has no settings dialog: it remembers what you set up in the window. The state is saved when
pserv closes, in `%APPDATA%\pserv6\config.toml`:

- window position and size
- the active view, theme, zoom and table font
- column widths, column order, hidden columns and sort for each view
- auto-refresh and its interval
- the last remote machine

The file is plain [TOML](https://toml.io); edit it only while pserv is not running. Deleting it
returns pserv to its defaults. `pservc` reads the same file but never writes it.

On the first start, pserv 6 imports the settings of pserv 5 if it finds them
(`%LOCALAPPDATA%\pserv5\pserv5.toml`), except the font size.

## Logs

`pserv6.log` and `pservc.log` are written to the same folder. When an action fails with "see the
log", this is where to look. **Help > About pserv6...** shows the folder, the version and whether
pserv runs as administrator.

## Self-test

`pserv6.exe --selftest [View,View,...] [--seconds N] [--log file]` loads, scrolls and refreshes
every view and writes a timing log; `--selftest-actions` runs the views' actions against
throwaway objects it creates itself. Both leave the settings untouched. They exist for
development and for checking an installation.
