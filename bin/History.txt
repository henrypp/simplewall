v2.3.11 (x December 2018)
- added "/install" argument for install filtering
- added cache auto clean up (to prevent overflow)
- changed minimum size of main window (issue #269)
- changed installation message
- increased rule parsing speed (issue #276)
- memory optimization
- fixed notification window appears on taskbar (regression)
- fixed exclude user rules option was not working
- cosmetic fixes
- fixed bugs

v2.3.10 (28 November 2018)
- fixed old bug where incorrect rules are may accepted as filters
- fixed bugs

v2.3.9 (20 November 2018)
- fixed resolving ip addresses may hang out net events thread (issue #256)
- fixed net events subscription when option is not set
- fixed protocol names
- fixed bugs

v2.3.8 (7 November 2018)
- fixed #251
- fixed #253

v2.3.7 (5 November 2018)
- fixed dropped events callback subscription on latest win10
- fixed notifications sound cannot be played in some cases
- fixed fastlock race condition (critical)
- cosmetic fixes
- fixed bugs

v2.3.6 (16 October 2018)
- added purgen submenu into tray menu
- added more statusbar information
- set extra large icons view by default
- tray menu services counting bug
- fixed app with overdue timer still enabled on profile load
- fixed resetting some data on profile load
- fixed timer resetting on profie load
- fixed listview checkboxes
- cosmetic fixes
- fixed bugs

v2.3.5 (14 October 2018)
- added extra large icons view
- use correct function for netevents subscription (win10rs4+)
- fixed forced cleartype font style (issue #233)
- fixed possible buffer overflow
- rerwritten timer engine
- cosmetic fixes

v2.3.4 (21 September 2018)
- fixed dns system rules when dns service is stopped
- fixed listen filter conditions (issue #213, #222)
- fixed memory referencing bug (issue #221)
- fixed massive handles leak (issue #230)
- fixed thread termination
- various ui fixes

v2.3.3 (8 July 2018)
- added system rules update feature
- fixed inaccurate timeout between notifications  (issue #194)
- fixed fullscreen apps showstopper (issue #209)
- fixed possible crash for device path resolution
- fixed skipuac warnings for some machines
- fixed some services path resolution
- fixed saving empty rules config
- fixed update engine (issue #182)
- updated system rules

v2.3.2 (27 Juny 2018)
- added sorting by state for user rules in app context menu
- increased priority for blocking user rules
- fixed loading on startup (issue #75)
- fixed editor apps list sorting
- fixed service names displaying

v2.3.1 (25 Juny 2018)
- fixed loopback rules (added more reserved ip addresses)
- fixed sometimes system cannot be going to sleep
- fixed applying rules for services (appcrash)
- fixed update sometimes cannot be installed
- fixed services enumeration
- fixed system rules

v2.3 (19 Juny 2018)
- added allowed connections monitoring in dropped packets log (win8+)
- added inbound multicast and broadcast connections logging (win8+)
- added outbound redirection filter layer (win7+)
- added separation for remote/local address/port in rules editor
- added hotkeys for import/export profile
- added win10 rs5 support
- prevent memory overflow for singly linked lists (win7+) (issue #193)
- do not load icons for processes if icons displaying are disabled
- improved multiple rules applying speed in settings window
- increased time limit for displaying same notification (win7+)
- search loading dlls in system directories only (safety)
- check for correct xml data type before loading
- store last notification timestamp for apps
- removed proxy support (win8+)
- fixed dropped events callback crash (win7+)
- fixed applying services filters
- fixed alphanumeric sorting
- improved port scanning defense
- improved loopback connections
- improved boot-time filters
- stability improvements
- cleanup xml atributes
- updated system rules
- cosmetics fixes
- fixed ui bugs
- fixed bugs

v2.2.12 (6 Juny 2018)
- added win10 rs4 support

v2.2.11 (6 Juny 2018)
- fixed double race condition lock (critical)

v2.2.10 (6 Juny 2018)
- added feature to disable special rules group (issue #181)
- revert special rules highlighting
- fixed listview focusing (issue #164)
- fixed switching modes
- fixed reported bugs
- code cleanup
- fixed bugs

v2.2.9 (5 Juny 2018)
- added exclude custom rules from notifications feature (issue #177)
- fixed app does not change group when special rule was removed
- fixed sometimes netevents cannot be unsubcribed
- fixed ipv6 addresses loopbacks
- fixed thread event synchronization
- fixed special rules group sorting
- cosmetics fixes
- fixed bugs

v2.2.8 RC (21 May 2018)
- fixed device changes notifications (issue #128)
- fixed fullscreen apps loses focus (issue #178)
- fixed saving new rules (issue #179)
- fixed loopback condition flag
- code cleanup

v2.2.7 RC (13 May 2018)
- removed search feature (do not used by anyone)
- stability improvements
- fixed loopback blocking (adobe software now working well when you enable loopback connections)
- fixed singly linked list structure alignment for 32-bit
- fixed timers does not applied from notification window (issue #172)
- fixed apps does not change sorting sometimes
- fixed apps does not change group sometimes
- fixed bugs

v2.2.6 RC (11 May 2018)
- fixed special rules does not change apps group
- fixed notifications indexing
- fixed notifications icon redraw
- fixed another dns caching
- fixed ui bugs
- fixed bugs

v2.2.5 Beta (4 May 2018)
- fixed saving profile on manual apps state changing
- fixed some untranslated strings
- fixed installer/uninstaller
- fixed rules checkboxes
- fixed ui bugs
- fixed bugs

v2.2.4 Beta (3 May 2018)
- now simplewall install filters fastest than Manco pulls out his revolver
- reworked update engine, now it check all components updates automatically
- sort rules alphabeticaly
- fixed threads priority race condition
- fixed applying filters without pause
- fixed dns caching (issue #153)
- fixed ui bugs
- fixed bugs

v2.2.3 Beta (16 April 2018)
- added drop settings to default feature
- changed log format to csv (comma-separated values) table
- changed install/uninstall filters button icon
- notifications: instant timer applying in notification window
- notifications: cosmetics ui (issue #150)
- notifications: restored "disable notifications for this app" button
- notifications: fixed notification buttons behavior
- notifications: removed timeout threshold
- changed current locale version calculation method (more precision)
- removed special rules highlighting (they have own group for it)
- updated to the latest pugixml 1.9
- cosmetics update engine
- fixed ui bugs
- fixed bugs

v2.2.2 Beta (26 March 2018)
- new update engine
- show full app paths in notifications when "show filenames only" is unchecked
- added grouping for apps with user rules
- added opening file properties feature
- cosmetics for the notification ui (issue #146)
- cosmetics for the apps menu
- fixed installer who does not removing profile backups
- fixed signature of apps cannot be checked at startup
- fixed dns resolution in some cases (issue #127)
- fixed various memory leaks because of icons resources
- fixed ui bugs
- fixed bugs

v2.2.1 Beta (14 March 2018)
- instant apps list sorting
- notifications: added information about blocked protocol
- notifications: replaced "disable notifications for this app" icon
- notifications: changed default timeout between same notifications
- notifications: tray popup sometimes won't shown on some systems
- notifications: ignore button combined with block button
- notifications: changed texts for remote/local addresses
- timer does not removed when user manually uncheck apps
- removed font boldening for itself (issue #135)
- changed minimal width of main window
- fixed timers formatting
- fixed ui bugs
- fixed bugs

v2.2 Beta (4 March 2018)
- new notification ui
- now simplewall added to the apps list automatically (issue #106)
- added windows services support [beta] (issue #88)
- added profile timestamping
- new localization engine (single .lng file)
- more sensitive notifications (issue #107)
- lock-free dropped events callback (win7+)
- added group total items count indication
- added block action for notifications (issue #123)
- automatic profile backup (issue #110)
- added network address resolution
- make internal apps undeletable
- menu bitmap transparent icons
- app paths case correction
- added timers (issue #96)
- set process high priority
- ipsec dropped packets logging (win8+)
- removed wow64 redirection (use simplewall 64-bit binaries for win64)
- revert "purge unused apps" feature
- optimized apps types recognition
- improved tray context menu (issue #103)
- improved memory allocation
- changed verify signatures algorithm (issue #94)
- changed "purge invalid apps" hotkey
- changed default font
- cosmetics for filter names
- cosmetic fixes (issue #108)
- stability improvements
- updated default colors
- updated localization
- fixed dropped events callback failure (win10 rs3 and above)
- fixed steal focus at startup and when notification displaying
- fixed working under blacklist mode
- fixed multi-monitor support
- fixed ui bugs
- fixed bugs

v2.1.4 (27 November 2017)
- do not verify signatures for store apps (win8+)
- optimized digital signatures verification (issue #94)
- fixed appcontainers listing (removed firewallapi.dll dependence) (win8+) (issue #104)
- fixed notifications race conditions (it may fix issue #73)
- fixed status does not changed when app deleted
- fixed "system" process marked as pico
- updated blocklist
- code cleanup
- fixed bugs

v2.1.3 (22 November 2017)
- disabled loopback and digital signatures config by default
- fixed displaying name of store apps (win8+) (issue #98)
- fixed network paths rules (issue #102)

v2.1.2 RC (21 November 2017)
- added option to disable apps signature checking
- set selected apps when you are open rules editor from main window
- reworked special rules (minimized memory usage and speed improvements, also removed limit in apps selection for special rules)
- removed ocsp signature verification (issue #94)
- improved apps version receiving
- renamed "filters" into "rules"
- fixed various rules editor crashes (issue #89)
- fixed notifications race conditions (it may fix issue #73)
- fixed blocklist incorrect check state
- fixed restoring after hibernation
- updated localization
- fixed ui bugs
- fixed bugs

v2.1.1 Beta (17 November 2017)
- reworked filter settings page
- added option to disable hosts support for rules
- added option to load blocklist extra rules
- cosmetic fixes for ipv6 address format
- fixed dns resolutions where it does not required (issue #94)
- fixed various rules editor crashes (issue #89)
- fixed windows store icon destroying
- removed filters configuration from menu (use settings dialog instead)
- removed internal rules files from distro
- updated internal rules
- updated localization
- fixed ui bugs
- fixed bugs

v2.1 Beta (12 November 2017)
- added windows store apps support (win8+)
- revert allowing loopback connection feature
- converted log limit unit to kilobytes
- dropped packets log cosmetic fixes
- improved confirmation dialogs
- updated localization
- fixed settings will not be applied for main menu
- fixed displaying icons for some processes
- fixed rules editor crash (issue #89)
- fixed color items reorganization
- fixed ui bugs
- fixed bugs

v2.0.20 (6 November 2017)
- now custom rules will overwrite system rules
- added warning message for listen connections option
- apply filters on demand in settings dialog
- added port support for ip ranges
- removed rules configuration from menu (use settings dialog instead)
- fixed rule apps does not saved when checkbox are checked
- fixed rule generation from notification window
- fixed listen connections does not blocked
- fixed highlighting special rules for apps
- fixed running under non-admin account
- fixed skip-uac working directory
- fixed listview sorting
- fixed ui bugs
- fixed bugs

v2.0.19 (1 November 2017)
- new rules editor ui
- added highlighting rules with errors
- automatically sorting rules after changing
- added feature to set custom dns ipv4 server ("DnsServerV4" in .ini)
- added option to exclude blocklist rules from notifications
- show process information in statusbar on menu item hover
- optimized signature information retrieving from binaries
- updated localization
- fixed saving profile in some cases
- fixed parsing rules types (issue #70)
- fixed dns queries
- fixed ui bugs
- fixed bugs

v2.0.18 (20 October 2017)
- added setting to disable proxy support (win8+)
- prevent notifications duplicate
- fixed windows firewall disabling on win10
- fixed notifications sound configuration does not saved
- fixed notifications sound does not played on some systems
- cosmetic fixes about notifications cross button
- updated localization
- updated blocklist
- fixed dpi support
- fixed ui bugs

v2.0.17 (12 October 2017)
- clear notifications cache on apply filters and configuration
- show more address information on notification window tooltip
- fixed redraw listview after notification actions
- fixed ui hangouts for a long time sometimes
- fixed race conditions
- fixed ui bugs
- fixed bugs

v2.0.16 (6 October 2017)
- fixed internal rules configuration saving

v2.0.15 (6 October 2017)
- make current settings backup before import
- changed default listview font
- fixed settings listview groups does not changed immediately
- fixed notification window display at startup
- fixed uninstaller do not removed some files

v2.0.14 (5 October 2017)
- apply settings on demand (too much faster)
- revert listen connections blocking feature (request)
- set font also for settings lists
- revert forgotten rules editor button
- updated localization
- fixed ui bugs

v2.0.13 (4 October 2017)
- new settings ui
- fixed settings reset on device arrival
- fixed colors configuration checkboxes

v2.0.12 RC (3 October 2017)
- use colors and tooltip for notification icon same as in main window
- do not show notifications on tray hover when it is disabled
- do not bring notifications window into the foreground when it shows
- revert error log tray menu
- fixed network paths detection
- fixed displaying non interesting errors
- fixed some device path conversions
- fixed notifications cleanup
- updated localization
- fixed ui bugs

v2.0.11 Beta (30 September 2017)
- subscribe for net events only when filters are installed (win7+)
- fixed incorrect filters applied for special rules
- fixed all apps are in lowercase

v2.0.10 Beta (29 September 2017)
- added selection counting for remove items dialog
- show signature information in notification app icon tooltip
- show exit confirmation dialog when filters are installing
- set low-limit for notification timeout
- revert forgotten privelege (regression)
- fixed filters for non existing paths (regression)
- fixed string case for cyrillic symbols
- fixed filters thread
- fixed minimize button
- fixed ui bugs
- fixed bugs

v2.0.9 Beta (25 September 2017)
- improved notifications cleanup
- revert processes icons
- fixed nt path conversions (regression)
- fixed notification ui display timeout
- fixed ui bugs
- fixed bugs

v2.0.8 Beta (21 September 2017)
- new logo
- changed minimal size of main window
- updated notification ui
- updated icons pack
- updated blocklist
- fixed font quality (use cleartype ever)
- fixed notifications sound
- fixed bugs

v2.0.7 Beta (14 September 2017)
- added count marks for all groups
- added font selection for listview
- prefer "blocklist_full.xml" parsing if it's presented
- improved signatures checking
- optimized listview redraw items
- optimized listview autosizing columns
- disable retrieving icon handle for network paths for notification dialog
- changed default listview colors
- optimized loading speed
- code cleanup
- fixed ui hanging in some cases
- fixed bugs

v2.0.6 Beta (11 September 2017)
- added signature checking for apps
- added set default language as in system
- changed default listview colors
- updated project sdk
- fixed allowed apps cannot recieve any data from network (when stealth-mode enabled)
- fixed retrieving nt path for reparse point files (issue #59)
- fixed update checking not working on some systems
- fixed bugs

v2.0.5 Beta (30 August 2017)
- added proxy support (win8+)
- added indication for inbound connections for all (when stealth-mode enabled)
- added group for special rules (rules applied for apps)
- added apps list selection indication
- changed experimental settings mark into "for experts only"
- removed process list menu icons
- improved retrieving file path by handle
- improved notifications ui
- updated project sdk
- fixed ui bugs
- fixed bugs

v2.0.4 Beta (23 August 2017)
- added grouping for rules
- removed listen connections blocking feature
- improved import/export xml database
- fixed open rules editor action in main window
- fixed crash on delete rules
- fixed ui bugs
- fixed bugs

v2.0.3 Beta (17 August 2017)
- added import/export applications list feature
- added mode selection into installation dialog
- added tooltips into the notifications ui
- added remembering collapsed state for the listview groups
- increased internal rules loading speed (please update blocklist.xml and rules_system.xml to latest versions)
- increased rules list icons size
- changed default sorting configuration
- fixed notifications ui logic
- fixed filters installation state flag
- fixed support some domains
- fixed carriage return type for rules_system.xml
- fixed thread spinlock
- updated localization
- updated project sdk
- updated blocklist
- fixed bugs

v2.0.2 Beta (7 August 2017)
- fixed incorrect vector index

v2.0.1 Beta (7 August 2017)
- added update checking for new beta version
- added flush dns cache after filters applied
- set max prefix length for ipv6 addresses to 64
- new rules editor interface
- changed minimum width for a main window
- fixed running with "/minimized" argument under uac
- updated localization
- fixed bugs

v2.0 Beta (1 August 2017)
- new notification ui
- show notifications only for whitelist mode
- allow listen connections for all is enabled by default
- added import custom rules from file feature
- added support to load large xml files
- added more information into the main window
- added apps grouping (allowed/blocked)
- added custom rules syntax checking
- added support dns resolution for custom rules
- added resolving shortcut path
- added notification display timeout config
- save internal rules configuration into xml
- purgen remove only apps with errors
- minimized dropped packets log size (union remote and local address information)
- minimized memory usage
- removed "trust no one" mode
- updated system rules
- updated blocklist
- fixed dropped packets logging hibernation (win7+)
- fixed remember windows size and position sometimes
- fixed version string trimming
- fixed ui bugs
- fixed memory leaks
- fixed bugs

v1.6.5 (1 June 2017)
- do not block listen connections on stealth-mode
- do not block listen connections on boot-time
- fixed dropped events does not shutdown on exit (win7+)
- fixed memory leak

v1.6.4 (31 May 2017)
- added fallback if blocklist and/or system rules not found
- added more dropped events logging (win7+)
- fixed dropped events subscription duplicate (win7+)
- fixed run as admin does not work sometimes
- updated blocklist
- fixed bugs

v1.6.3 (27 May 2017)
- generate unique session key at startup
- fixed custom app rules crash on delete
- fixed lookup account sid length mismatch
- fixed dropped packets logging crash (win7+)
- stability improvements
- updated system rules
- fixed bugs

v1.6.2 (24 May 2017)
- create filter even if file doesn't exists (drive must be mounted)
- allow inbound & listen traffic only if stealth-mode does not enabled
- added required rules for the ipv6 stack to work properly
- stealth-mode marked as experimental
- fixed com library initialization (again!)
- fixed stealth-mode
- fixed bugs

v1.6.1 (23 May 2017)
- added username and domain information to the window title
- added configuration refresh on user logon
- removed static wfp session key (request)
- fixed com library initialization
- fixed incorrect return value on file not found error

v1.6 (19 May 2017)
- added stealth-mode (to prevent udp/tcp port scanning)
- added acl (access control list) to the engine
- added gridline for the listview config
- added item into the custom rules menu for open rules editor
- added version-independent network events api call (win7+)
- added dropped packets log file size limit to 1mb (win7+)
- reset windows firewall to its initial state when restore it back
- blocklist marked as experimental
- removed custom rules from package
- fixed dropped packets logging stop sometimes (win7+)
- fixed removing custom rules
- fixed classic ui
- fixed bugs
- ui fixes

v1.5.5 (6 May 2017)
- added installer
- added static wfp session key
- copy filter name if description is not available for dropped packets log
- removed "file not found" xml parsing errors
- revert trim rules back (request)
- fixed index flag cannot be set (win8+)
- fixed ui bugs

v1.5.4 (30 April 2017)
- trim executable version string
- fixed filters uninstallation
- fixed duplicate update checking
- fixed bugs

v1.5.3 (27 April 2017)
- fixed restoring windows firewall state
- fixed incorrect window size at startup sometimes

v1.5.2 (27 April 2017)
- fixed dropped packets log spinlock cannot be unlocked (critical)
- fixed displaying "file not found" errors
- restore listview selection after application delete
- disable main button on filters installation
- improved shutting down windows firewall feature
- removed unnecessary whitespace trims
- optimized window resizing
- updated to the latest sdk
- fixed bugs

v1.5.1 (17 April 2017)
- added remember window position and size feature
- added "enable errors notifications" config
- added f11 hotkey for maximize window
- disable wow64 filesystem redirection
- changed error log notification text
- fixed possible memory leak

v1.5 (15 April 2017)
- added index flag to the filters, to help enable faster lookup during classification (win8+)
- added app container loopback traffic permission (win8+)
- added "allow listen connections for all" config
- added loopback indication for dropped packets log
- copy real path instead display path on copy command in main window listview
- do not show dropped packets notifications when filters are not installed
- if boot-time filters enabled then apply system rules for boot-time too
- custom rules for apps does not saved sometimes
- removed running without admin rights feature
- changed notification about errors logic
- changed alt+f4 behaviour (request)
- cosmetic fixes for tooltips
- fixed disabling windows firewall on some systems
- fixed settings tabstop doesn't work
- fixed incorrect listview icons for some apps
- fixed process list some apps have no icons
- fixed possible duplicate filters
- fixed purge unused apps
- stability improvements
- updated translations
- updated system rules
- updated pugixml
- fixed bugs

v1.4.6 (5 April 2017)
- added write error logs into a file feature
- fixed process list does not recognize pico applications on win10
- updated translations
- fixed bugs

v1.4.5 (4 April 2017)
- added pico support (subsystem for unix-based applications) for win10
- added l2tp/ipsec for system rules
- added localhost for custom rules
- fixed access denied for some self protected applications (like "ekrn.exe" for nod32)

v1.4.4 (28 March 2017)
- added ipsec connections monitoring into the log (win8+)
- cosmetic fixes about tray notifications
- fixed dropped packets callback crash (critical)
- fixed displaying tray notifications on win10
- fixed possible duplicate filters

v1.4.3 (27 March 2017)
- added provider information into the log/notifications
- added displaying error messages on filters configuration via tray popup
- fixed link processing in settings window
- cosmetic changes about debug error messages
- removed "nlatunicast" flag from loopback permissions
- updated blocklist
- code cleanup

v1.4.2 (19 February 2017)
- added hints to rules pages
- fixed filter creation for nonexistent apps (critical)
- fixed resource definition
- fixed purge unused apps
- cosmetic changes about rules tooltip

v1.4.1 (4 February 2017)
- fixed suspended drop event callback (critical)
- fixed suspended apply filters thread (critical)
- fixed purge unused apps

v1.4.0 (31 January 2017)
* revert original project name
- added mode changing confirmation
- added reading information about "System" process
- added protocol and version (ports only) option into the rules editor
- added blocklist editor (set it "on" or "off" only)
- added custom rules applying to the apps feature
- added "show filenames only" option
- added icon indication for rules
- added loopback permission for "trust no one" mode
- moved system rules into the "rules_system.xml" file
- clear log even if logging to a file is not enabled
- do not load information about apps from shared resources
- fixed profile not saved if filters is not installed
- removed tray balloon tips on filters changing
- boot-time filters marked as experimental
- improved working under uac (no rights)
- improved system apps detection
- cosmetic fixes in process list
- cosmetic fixes in apps tooltip
- fixed race conditions
- updated translation
- updated blocklist
- updated ui
- fixed bugs

v1.3.7 (21 November 2016)
- fixed special rules crash on apply settings
- fixed special rules reinitialization in main menu/tray menu
- updated translation

v1.3.6 (20 November 2016)
- added loopback permission for boot-time filters
- added ip:port syntax for special rules
- added default values for new special rules
- fixed restore listview selection
- fixed special rules crash on delete item
- fixed log path unexpand environment strings

v1.3.5 (17 November 2016)
- fixed configuration saving (critical)
- fixed xml saving (critical)

v1.3.3 (14 November 2016)
- added boot-time filters for prevent data leak during system startup, even before "Base Filtering Engine" (BFE) service starts
- added "purge unused applications" feature
- added "hide icons" feature
- added documentation for rules editor and filter page
- added special rules to main menu/tray menu
- fixed multi-select configuration in listview
- fixed system imagelist destroying
- fixed dropped packets filters decription empty sometimes
- fixed find dialog crash
- removed inbound events logging for some reason
- updated translation
- updated pugixml

v1.3.2 (29 October 2016)
- added domain\username indication to log events
- added filter name to log events
- added ip/port range support for rules
- combined ip and port rules settings page
- added save checkbox state on install/delete message
- added listen loopback permission
- added inbound events logging
- set xml load encoding to auto
- improved logic for detect incorrect applications
- listview empty text indication doesn't changed at locale change
- changed default log parameters for "system" & "svchost.exe"
- fixed inbound ip doesn't affected in applied rules
- changed default color for silent applications
- removed output log into debug log feature
- improved open/save file dialog flags
- optimized log callback
- updated blocklist
- updated translation
- minor improvements

v1.3.1 (21 October 2016)
- added option to set application for open log file
- added option to exclude application from log
- added option for disable notification sound
- added middle click on tray icon now open log file
- added empty listview text indication
- added rules highlighting
- added usage examples to rules.xml 
- reorganized log settings
- fixed unit of time for notification timeout had in minutes (instead seconds)
- fixed special rules saved twice
- fixed skip user account control not worked
- fixed special rules saved empty when filters not installed
- fixed system rules cannot be unchecked automatically
- changed auto byte order mask for xml load/save
- replaced QueryFullProcessImageName to NtQueryInformationProcess
- stability improvements
- updated translation
- ui improvements

v1.3 (15 October 2016)
- disable/enable windows firewall on filters installation/deletion
- normalize nt paths for dropped packets callback
- exclude "svchost.exe" & "system" from log configuration
- abbility to show/clear log
- resize support as in alpha builds
- hotkeys for menu items
- PathUnExpandEnvStrings for log path
- confirmation settings for exit/deleting application/log clear
- changed message boxes style
- changed default color for invalid applications
- fixed access denied for some processes
- config doesn't saved if user don't trigger apply filters
- improved windows firewall control
- stability improvements
- updated translation

v1.2.118 (5 October 2016)
- fixed crash on filters installation
- save sort order into profile (regression)
- unable to clear log path config
- stability improvements
- updated translation

v1.2.117 (4 October 2016)
* changed ui by IAEA safety standards (issue #2)
- added logging configuration
- now filters disabled by default
- inbound ports doesn't blocked for special rules
- updated translation
- minor improvements

v1.1.116 (30 September 2016)
- added "listen" layer blocking
- added forgotten rules into settings menu
- added open folder by double click for listview
- added shared resources highlighting
- dropped packets logging optimizations
- fixed process list menu icons with classic ui
- updated translation
- minor improvements

v1.1.115 (22 September 2016)
- added more information to dropped packets logging (win7+)
- fixed inbound dont't blocking
- cannot add port in rules editor
- updated translation

v1.1.114 (21 September 2016)
- rules in settings dialog cannot be saved

v1.1.113 (20 September 2016)
- fixed cannot delete application from list bug
- fixed default values for settings
- filters for services do not set
- fixed dhcpv6 ports configuration
- fixed windows firewall configuration
- updated translation
- fixed bugs

v1.1.112 (19 September 2016)
* project renamed
- added "trust no one" mode
- added dropped packets logging to debugview (win7+)
- added automatic rules applying on insert device
- added name for filters
- moved telemetry rules to the resources
- rewritten rules editor
- improved classic ui config
- fixed tray tooltip visibility
- decreased ballon tips

v1.0.101 (7 September 2016)
- added outbound connections configuration for rules
- added color configuration
- changed weight for sublayer
- removed startup popup notification
- updated translation

v1.0.100 (30 August 2016)
- added individual applicaiton rules
- added highlight for invalid items
- added icons for process popup menu
- rewritten process retrieving code (fixed many bugs)
- restored "Refresh" listview feature
- prevent for duplicate filters on some configurations
- unlocked application layer configuration in "for all (global)" rules
- loaded default rules on initilize settings
- updated translation
- fixed bugs

v1.0.99 (25 August 2016)
- added windows update and http protocol allow rule
- blocking list editor context menu does not showed
- blocking list editor first item cannot be edited
- fix rules checkbox displaying in settings window
- rules re-grouped by OSI model
- changed listview custom draw
- updated translation
- updated ui

v1.0.98b (22 August 2016)
- added ballon tip on remove filters
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
- now compiled with "treat warnings as error" parameter
- added global rules configuration
- added "disable windows firewall" param
- added balloon tips param
- started localization
- fixed buffer overflow on remove filters
- finished blocking list editor
- removed duplicate filters
- fixed memory leaks on change window icon
- ui improvements
- now checking updates worked
- fixed statusbar redraw forgotten
- updated translation
- ui fixes

v1.0.81a (18 August 2016)
- added indication allowed/blocked item count
- added edit blocking list support
- added loopback configuration into profile
- fixed profile mask set to default at startup
- improved profile configuration dialog

v1.0.79a (17 August 2016)
- added tooltips
- inbound traffic not blocked sometimes

v1.0.77a (16 August 2016)
- added profiles for applications
- added more hotkeys
- find now worked
- increased startup speed (moved filters applying function into another thread)
- fixed ntp setting
- fixed memory leak
- ui improvements

v1.0.52a (13 August 2016)
- fixed main window dpi
- fixed scroll height on change icon size
- removed comments/debug strings
- fixed application state does not saved on check/uncheck
- added "select all" (ctrl+a) hotkey
- added "telemetry.xml"
- ip parsing optimization (due ParseNetworkString)
- ui improvements

v1.0.42a (12 August 2016)
- added outbound/inbound ICMP permission
- auto apply filters on settings change
- removed "Apply rules" button (do not need it anymore)
- increased startup speed
- removed process monitor
- changed config defaults
- ui improvements
- small bug fixes

v1.0.30a (11 August 2016)
- added outbound loopback permission
- fixed DHCP/DNS/NTP allowed for all
- fixed memory leak

v1.0.23a (6 August 2016)
- added support to allow DHCP/DNS/NTP through svchost.exe
- fixed NtOpenProcess access rights

v1.0.21a (4 August 2016)
- fixed uncheck item doesn't saved
- previous build changes extended
- removed duplicate function calling
- menu fixes

v1.0.18a (4 August 2016)
- added internal telemetry blocking
- added menu accelerators
- moved process monitor to low-level
- fixed provider persistency
- re-fuck-to-ring

v1.0.13a (2 August 2016)
- added search abbility for existing application filters and show them
- added tray icon indication
- added find in application list feature
- increased speed (due transactions)
- fixed 32-bit working under 64-bit system
- ui improvements

v1.0.10a (30 July 2016)
- fixed xml saving/parsing bug
- fixed read memory access on filter enumeration

v1.0.7a (29 July 2016)
- added inbound loopback permission
- added balloon tips on events
- fixed inbound permission for allowed applications
- fixed critical bug (incorrect memory address access)
- common optimizations
- ui improvements

v1.0.4a (27 July 2016)
- added icons size configuration
- fixed background thread to catch applications doesn't worked
- fixed v6 callout misstake
- ui improvements

v1.0.3a (26 July 2016)
- added more settings
- improved ui
- removed service (do not need it anymore)

v1.0.2a (26 July 2016)
- first public version
