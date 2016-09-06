v1.0.101 (7 September 2016)
+ added outbound connections configuration for rules
+ added color configuration
- changed weight for sublayer
- removed startup popup notification
- updated translation

v1.0.100 (30 August 2016)
+ added individual applicaiton rules
+ added highlight for invalid items
+ added icons for process popup menu
- rewritten process retrieving code (fixed many bugs)
- restored "Refresh" listview feature
- prevent for duplicate filters on some configurations
- unlocked application layer configuration in "for all (global)" rules
- loaded default rules on initilize settings
- updated translation
- fixed bugs

v1.0.99 (25 August 2016)
+ added windows update and http protocol allow rule
- blocking list editor context menu does not showed
- blocking list editor first item cannot be edited
- fix rules checkbox displaying in settings window
- rules re-grouped by OSI model
- changed listview custom draw
- updated translation
- updated ui

v1.0.98b (22 August 2016)
+ added ballon tip on remove filters
- changed default rules for all
- changed default rules for allowed

v1.0.96b (21 August 2016)
- settings improvements
- fixed duplicate popup on startup
- cleanup code/resources
- updated translation

v1.0.94b (20 August 2016)
- updated translation
- removed excess tray popups
- filters not applied when firewall started manually
- tray icon does not change on firewall enable
- improved block list editor

v1.0.90b (19 August 2016)
+ now compiled with "treat warnings as error" parameter
+ added global rules configuration
+ added "disable windows firewall" param
+ added balloon tips param
+ started localization
- fixed buffer overflow on remove filters
- finished blocking list editor
- removed duplicate filters
- fixed memory leaks on change window icon
- ui improvements
+ now checking updates worked
- fixed statusbar redraw forgotten
- updated translation
- ui fixes

v1.0.81a (18 August 2016)
+ added indication allowed/blocked item count
+ added edit blocking list support
+ added loopback configuration into profile
- fixed profile mask set to default at startup
- improved profile configuration dialog

v1.0.79a (17 August 2016)
+ added tooltips
- inbound traffic not blocked sometimes

v1.0.77a (16 August 2016)
+ added profiles for applications
+ added more hotkeys
+ find now worked
- increased startup speed (moved filters applying function into another thread)
- fixed ntp setting
- fixed memory leak
- ui improvements

v1.0.52a (13 August 2016)
- fixed main window dpi
- fixed scroll height on change icon size
- removed comments/debug strings
- fixed application state does not saved on check/uncheck
+ added "select all" (ctrl+a) hotkey
+ added "telemetry.xml"
- ip parsing optimization (due ParseNetworkString)
- ui improvements

v1.0.42a (12 August 2016)
+ added outbound/inbound ICMP permission
+ auto apply filters on settings change
- removed "Apply rules" button (do not need it anymore)
- increased startup speed
- removed process monitor
- changed config defaults
- ui improvements
- small bug fixes

v1.0.30a (11 August 2016)
+ added outbound loopback permission
- fixed DHCP/DNS/NTP allowed for all
- fixed memory leak

v1.0.23a (6 August 2016)
+ added support to allow DHCP/DNS/NTP through svchost.exe
- fixed NtOpenProcess access rights

v1.0.21a (4 August 2016)
- fixed uncheck item doesn't saved
- previous build changes extended
- removed duplicate function calling
- menu fixes

v1.0.18a (4 August 2016)
+ added internal telemetry blocking
+ added menu accelerators
+ moved process monitor to low-level
- fixed provider persistency
- re-fuck-to-ring

v1.0.13a (2 August 2016)
+ added search abbility for existing application filters and show them
+ added tray icon indication
+ added find in application list feature
- increased speed (due transactions)
- fixed 32-bit working under 64-bit system
- ui improvements

v1.0.10a (30 July 2016)
- fixed xml saving/parsing bug
- fixed read memory access on filter enumeration

v1.0.7a (29 July 2016)
+ added inbound loopback permission
+ added balloon tips on events
- fixed inbound permission for allowed applications
- fixed critical bug (incorrect memory address access)
- common optimizations
- ui improvements

v1.0.4a (27 July 2016)
+ added icons size configuration
- fixed background thread to catch applications doesn't worked
- fixed v6 callout misstake
- ui improvements

v1.0.3a (26 July 2016)
+ added more settings
- improved ui
- removed service (do not need it anymore)

v1.0.2a (26 July 2016)
- first public version
