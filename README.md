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
Simple tool to configure [Windows Filtering Platform (WFP)](https://docs.microsoft.com/en-us/windows/win32/fwp/windows-filtering-platform-start-page) which can configure network activity on your computer.

The lightweight application is less than a megabyte, and it is compatible with Windows 7 SP1 and higher operating systems.
You can download either the installer or portable version. For correct working you are require administrator rights.

### Nota bene:
Keep in mind, simplewall is not a control UI over Windows Firewall, and does not interact in any level with Windows Firewall. It works over Windows Filtering Platform (WFP) which is a set of API and system services that provide a platform for creating network filtering applications. Windows Filtering Platform is a development technology and not a firewall itself, but simplewall is the tool that uses this technology.

### Features:
- Simple interface without annoying pop ups
- [Rules editor](https://github.com/henrypp/simplewall/wiki/Rules-editor) (create your own rules)
- [Internal blocklist](https://github.com/crazy-max/WindowsSpyBlocker/wiki/dataSimplewall) (block Windows spy / telemetry)
- Dropped packets information with notification and logging to a file feature (win7+)
- Allowed packets information with logging to a file feature (win8+)
- Windows Subsystem for Linux (WSL) support
- Windows Store support
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
When you uninstall simplewall, all previously configured filters stay alive in system.
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
A: By default, simplewall blocks **all** applications, you do not need to create custom rules to block specific application.

#### Q: Is it safe to use simplewall with Windows Firewall?
A:  Yes. You do not need to disable Windows Firewall. This two firewall works independently.

#### Q: How can i disable blocklist entirely?
A:  Open `Settings` -> `Blocklist` and then click radio buttons labeled `Disable`.

#### Q: Where is blacklist mode?
A: Blacklist was removed many days ago for uselessness. But if you need it, you can still configure it.

<details>
<summary>Solution: Configure blacklist mode in simplewall:</summary>

---
1) Open `Settings` -> `Rules`
2) Uncheck `Block outbound for all` and `Block inbound for all` options.
3) Create user rule (green cross on toolbar) with block action, any direction, `Block connection` name and empty remote and local rule.
4) You can assign this rule for apps whatever you want to block network access.
</details>

#### Q: Why does my network icon have an exclamation mark?
A: When you are connected to a network, Windows checks for internet connectivity using Active Probing. This feature is named as NCSI (Network Connectivity Status Indicator). You can resolve this by one of this ways:

<details>
<summary>Solution 1: Enable NCSI through internal system rule:</summary>

---
1) Open `System rules` tab.
2) Allow `NCSI` rule (enabled by default).
</details>

<details>
<summary>Solution 2: Disable NCSI through system registry:</summary>

---
Create `Disable NCSI.reg` and import it into registry.

```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\NetworkConnectivityStatusIndicator]
"NoActiveProbe"=dword:00000001
"DisablePassivePolling"=dword:00000001
```
</details>

<details>
<summary>Solution 3: Disable NCSI through group policy:</summary>

---
1) Launch the group policy editor (`gpedit.msc` ).
2) Go to `Computer Configuration -> Administrative Templates -> System -> Internet Communication Management -> Internet Communication Settings`.
3) Double-click `Turn off Windows Network Connectivity Status Indicator active tests` and then select Enabled. Click Ok.
4) Open the Command Prompt (Admin) and enter `gpupdate /force` to enforce the changes made to the Group Policies.
</details>

#### Q: How can i disable Windows Firewall?
Start command line as administrator, and enter commands:

<details>
<summary>Disable Windows Firewall profiles:</summary>

---
~~~bat
netsh advfirewall set allprofiles state off
~~~
</details>

<details>
<summary>Enable Windows Firewall profiles:</summary>

---
~~~bat
netsh advfirewall set allprofiles state on
~~~
</details>

#### Q: How can i view all filters information?
Start command line as administrator, and enter commands:

<details>
<summary>Dump WFP filters and it's state:</summary>

---
~~~bat
cd /d %USERPROFILE%\Desktop

netsh wfp show filters
netsh wfp show state
~~~
</details>

- Filters information saved into `filters.xml` file.
- Filters, providers, callouts and layers for _ALL firewalls_ saved into `wfpstate.xml` file.

Open it in any text editor and study.

#### Q: Other questions:
- [How to fix Windows Update and Windows Store internet access (temporary solution)](https://github.com/henrypp/simplewall/issues/206#issuecomment-439830634)
- [Windows Security center integration (impossible)](https://stackoverflow.com/questions/3698285/how-can-i-tell-the-windows-security-center-that-im-an-antivirus/3698375#3698375)

Website: [www.henrypp.org](https://www.henrypp.org)<br />
Support: support@henrypp.org<br />
<br />
(c) 2016-2021 Henry++
