# Actions and properties

[Back to the contents](index.md)

## Toolbar

The top row shows every action of the current view as an icon button; point at a button to read
its name. Buttons for actions that the selected rows do not allow are greyed out - a stopped
service has no Stop - so you always see what a view can do. Related actions share one button with
a menu: **Copy**, **Set Priority** and **Set Startup**.

When the window is too narrow, the buttons that do not fit are in the **...** menu at the end,
with their names.

## Context menu

Right-click a row, or press `Shift+F10` or the menu key. The menu lists the same actions with
their names, followed by the copy and export commands. With several rows selected, each entry
shows the count, and an action is available only if every selected row allows it.

## Confirmation and progress

Destructive actions - deleting, uninstalling, terminating - are shown in red and ask for
confirmation first; **No** is the default button.

Actions that take time, such as starting or stopping services, show a progress bar with the name
of the item being worked on and a **Cancel** button above the status bar. The table stays visible
and usable.

If an action fails, pserv says why. The usual reason is missing
[administrator rights](getting-started.md#administrator-rights).

## The Properties dialog

Double-click a row or press `Enter`. The dialog lists every column of the item, including those
hidden in the table.

- Fields with a **bold** label can be edited: text boxes, number boxes or drop-down lists.
  **OK** applies the changes, **Cancel** discards them.
- With several rows selected, a picker at the top switches between the items; **OK** applies the
  edits of all of them.
- The item's actions are available as icon buttons below the fields. They are disabled while
  there are unsaved edits.

What can be edited:

| View | Editable |
|------|----------|
| Services, Devices | display name, start type, binary path, description |
| Environment Variables | name, value |

## Delete key

`Delete` runs the view's Delete action for the selected rows, with the usual confirmation.
