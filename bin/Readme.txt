simplewall

Description:
Simple tool to configure Windows Filtering Platform (WFP) which can configure network activity on your computer.

The lightweight application is less than a megabyte, and it is compatible with Windows 8.1 and higher operating systems.
You can download either the installer or portable version. For correct working you are require administrator rights.

System requirements:
- Windows 8.1 and above operating system.
- Visual C++ 2022 Redistributable package (https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

Nota bene:
Keep in mind, simplewall is not a control UI over Windows Firewall, and does not interact in any level with Windows Firewall. It works
over Windows Filtering Platform (WFP) which is a set of internal API and system services that provide a platform for creating network filtering
applications. Windows Filtering Platform is a development technology and not a firewall itself, but simplewall is the tool that uses this technology.

Command line:
List of arguments for simplewall.
-install - enable filtering (you can set "-silent" argument to skip prompt)
-uninstall - remove all installed filters

Uninstall:
When you uninstall simplewall, all previously installed filters are stay alive in system.
To remove all filters created by simplewall, start simplewall and press "Disable filters" button.

Features:
- Simple interface without annoying pop ups
- Rules editor (create your own rules)
- Internal blocklist rules (block Windows spy / telemetry)
- Dropped packets information with notification and logging to a file feature (win7+)
- Allowed packets information with logging to a file feature (win8+)
- Windows Subsystem for Linux (WSL) support
- Windows services support
- Windows Store support
- Free and open source
- Localization support
- IPv6 support

To activate portable mode, create "simplewall.ini" in application folder, or move it from "%APPDATA%\Henry++\simplewall".

Installation:
When install rules, you can choose two modes:
- Permanent rules - rules are working until you disable it manually.
- Temporary rules - rules are reset after the next reboot.

Uninstall:
When you uninstall simplewall, all previously configured filters stay alive in system.
To remove all filters created by simplewall, start simplewall and press "Disable filters" button.

Command line:
-install - enable filtering.
-install -temp - enable filtering until next reboot.
-install -silent - enable filtering without prompt.
-uninstall - remove all installed filters.

Website: [github.com/henrypp](https://github.com/henrypp)<br />
Support: sforce5@mail.ru<br />

(c) 2016-2023 Henry++
