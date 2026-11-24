# Remote machines

[Back to the contents](index.md)

pserv can show and control the services and devices of another computer.

1. Choose **File > Connect to Remote Machine...**
2. Enter the computer's name or IP address.
3. pserv tests the connection before it switches. If the test fails, it says why and stays on the
   current machine.

While connected:

- The badge in the title bar shows "Remote:" and the machine.
- Only **Services** and **Devices** are available; the other views are disabled.
- Start, Stop, Restart, Pause, Resume, the startup type, Uninstall Service and edits in the
  Properties dialog apply to the remote machine.
- Actions that need this computer - Open in Registry Editor, Open in Explorer, Open Terminal Here
  and Delete Registry Key - are not available for remote rows.

**File > Connect to Local Machine** returns to this computer. pserv remembers the last machine
and connects to it again at the next start.

## Requirements

- Administrative rights on the target machine
- Network access to its service control manager (the same access `services.msc` and `sc \\name`
  need)
