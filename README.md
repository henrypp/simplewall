simplewall [![Github All Releases](https://img.shields.io/github/downloads/henrypp/simplewall/total.svg)](https://github.com/henrypp/simplewall/releases) [![GitHub issues](https://img.shields.io/github/issues-raw/henrypp/simplewall.svg)](https://github.com/henrypp/simplewall/issues) [![Donate via PayPal](https://img.shields.io/badge/donate-paypal-red.svg)](https://www.paypal.me/henrypp/15) [![Donate via Bitcoin](https://img.shields.io/badge/donate-bitcoin-red.svg)](https://blockchain.info/address/1LrRTXPsvHcQWCNZotA9RcwjsGcRghG96c) [![Licence](https://img.shields.io/badge/license-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.en.html)
=======
#### Definitely for advanced users.
-------

[![simplewall](https://www.henrypp.org/images/simplewall.png?cachefix)](https://github.com/henrypp/simplewall/issues/250)

### Description:
Simple tool to configure Windows Filtering Platform (WFP) which can configure network activity on your computer.

The lightweight application is less than a megabyte, and it is compatible with Windows Vista and higher operating systems.
You can download either the installer or portable version. For correct working, need administrator rights.

#### Command line:
List of arguments for `simplewall.exe`:
- `/install` - enable filtering (you can set `/silent` argument to skip prompt)
- `/uninstall` - disable filtering

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

#### FAQ:
- [Is it safe to use simplewall with Windows Firewall?](https://github.com/henrypp/simplewall/issues/254#issuecomment-447436527)
- [Are internet connections blocked when simplewall is not running?](https://github.com/henrypp/simplewall/issues/119#issuecomment-364003679)
- [How can i disable blocklist entirely?](https://github.com/henrypp/simplewall/issues/243)
- [How to fix Windows Update and Windows Store internet access (temporary solution)](https://github.com/henrypp/simplewall/issues/206#issuecomment-439830634)
- [How to fix IDM Browser Integration](https://github.com/henrypp/simplewall/issues/111)
- [How to remove Windows Security center warnings](https://www.howtogeek.com/244539/how-to-disable-the-action-center-in-windows-10/) [win10 1607+](https://serverfault.com/a/880672)
- [Windows Security center integration (impossible)](https://stackoverflow.com/questions/3698285/how-can-i-tell-the-windows-security-center-that-im-an-antivirus/3698375#3698375)

Website: [www.henrypp.org](https://www.henrypp.org)<br />
Support: support@henrypp.org<br />
<br />
(c) 2016-2019 Henry++
