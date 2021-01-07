<h1 align="center">simplewall</h1>

<p align="center">
	<a href="https://github.com/henrypp/simplewall/releases"><img src="https://img.shields.io/github/v/release/henrypp/simplewall?style=flat-square&include_prereleases&label=version" /></a>
	<a href="https://github.com/henrypp/simplewall/releases"><img src="https://img.shields.io/github/downloads/henrypp/simplewall/total.svg?style=flat-square" /></a>
	<a href="https://github.com/henrypp/simplewall/issues"><img src="https://img.shields.io/github/issues-raw/henrypp/simplewall.svg?style=flat-square&label=issues" /></a>
	<a href="https://github.com/henrypp/simplewall/graphs/contributors"><img src="https://img.shields.io/github/contributors/henrypp/simplewall?style=flat-square" /></a>
	<a href="https://github.com/henrypp/simplewall/blob/master/LICENSE"><img src="https://img.shields.io/github/license/henrypp/simplewall?style=flat-square" /></a>
</p>

<p align="center">
	<i>Definitely for advanced users.</i>
</p>

-------

<p align="center">
	<img src="https://www.henrypp.org/images/simplewall.png" />
</p>

### Description:
Simple tool to configure Windows Filtering Platform (WFP) which can configure network activity on your computer.

The lightweight application is less than a megabyte, and it is compatible with Windows 7 and higher operating systems.
You can download either the installer or portable version. For correct working, need administrator rights.

### Features:
- Simple interface without annoying pop ups
- [Rules editor](https://github.com/henrypp/simplewall/wiki/Rules-editor) (create your own rules)
- [Internal blocklist](https://github.com/crazy-max/WindowsSpyBlocker/wiki/dataSimplewall) (block Windows spy / telemetry)
- Dropped packets information with notification and logging to a file feature (win7+)
- Allowed packets information with logging to a file feature (win8+)
- Windows Subsystem for Linux (WSL) support (win10)
- Windows Store support (win8+)
- Windows services support
- Free and open source
- Localization support
- IPv6 support

```
To activate portable mode, create "simplewall.ini" in application folder, or move it from "%APPDATA%\Henry++\simplewall".
```

### Installation:
When install rules, you can choose two modes:
- Permanent rules. Rules are working until you <a href="#uninstall">disable it manually</a>.
- Temporary rules. Rules are reset after the next reboot.

### Uninstall:
When you uninstall simplewall, all previously installed filters are stay alive in system.
To remove all filters created by simplewall, start simplewall and press "Disable filters" button.

### Command line:
List of arguments for `simplewall.exe`:

~~~
-install - enable filtering.
-install -temp - enable filtering until reboot.
-install -silent - enable filtering without prompt.
-uninstall - remove all installed filters.
~~~

### FAQ:
#### Q: Are internet connections blocked when simplewall is not running?
A: Yes. Installed filters are working even if simplewall is terminated.

#### Q: What apps are blocked in default configuration?
A: By default simplewall block **all** applications, you do not need create custom rules to block specific application.

#### Q: Why does my network icon have an exclamation mark?
A: When you are connected to a network, Windows checks for internet connectivity using Active Probing. This feature is named as NCSI (Network Connectivity Status Indicator). You can resolve this by one of this ways:

- You can allow NCSI rule in "System rules" tab (enabled by default).
- You can disable NCSI throught system registry:

~~~reg
; Create "Disable NCSI.reg" and import it into registry.

[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\NetworkConnectivityStatusIndicator]
"NoActiveProbe"=dword:00000001
"DisablePassivePolling"=dword:00000001
~~~

- You can disable NCSI throught group policy:

> 1) Launch the editor by typing in `gpedit.msc` in Run.
> 2) Navigate to `Computer Configuration -> Administrative Templates -> System -> Internet Communication Management -> Internet Communication Settings`
> 3) Double-click `Turn off Windows Network Connectivity Status Indicator active tests` and then select Enabled. Click Ok.
> 4) Now open the Command Prompt and enter `gpupdate /force` to enforce the changes made to the Group Policies.

#### Q: Where is blacklist mode?
A: Blacklist is removed many days ago for uselessness. But if you need it back you can configure blacklist in that way:

> 1) Open `Settings` -> `Rules`
> 2) Uncheck `Block outbound for all` and `Block inbound for all` options.
> 3) Create user rule (green cross on toolbar) with block action, any direction, `Block connection` name and empty remote and local rule.
> 4) You can assign this rule for apps whatever you want to block network access.

- Q: [Is it safe to use simplewall with Windows Firewall?](https://github.com/henrypp/simplewall/issues/254#issuecomment-447436527)
- Q: [How can i disable blocklist entirely?](https://github.com/henrypp/simplewall/issues/243)
- Q: [How to fix Windows Update and Windows Store internet access (temporary solution)](https://github.com/henrypp/simplewall/issues/206#issuecomment-439830634)
- Q: [How to fix IDM Browser Integration](https://github.com/henrypp/simplewall/issues/111)
- Q: [How to remove Windows Security center warnings](https://www.howtogeek.com/244539/how-to-disable-the-action-center-in-windows-10/) [win10 1607+](https://serverfault.com/a/880672)
- Q: [Windows Security center integration (impossible)](https://stackoverflow.com/questions/3698285/how-can-i-tell-the-windows-security-center-that-im-an-antivirus/3698375#3698375)

Website: [www.henrypp.org](https://www.henrypp.org)<br />
Support: support@henrypp.org<br />
<br />
(c) 2016-2021 Henry++
