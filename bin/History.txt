v3.6.1 (11 November 2021)
- added clear search on escape button
- fixed crash on delete filters (issue #1084)
- fixed import profile warning (issue #1086)
- fixed crash dumps processing
- updated locales
- cosmetic fixes

v3.6 (9 November 2021)
- improved multi-threading safety
- improved startup time
- improved dpi support
- added option to confirm allowing applications (issue #1070)
- added filtering for the application list (issue #663)
- added editor list items count mark for tab title
- added wfp initialization failure workaround
- added filtering for editor apps and rules
- added layer name into log
- moved log exclude configuration into another settings page (issue #1064)
- revert notification x button (issue #973)
- changed create rule toolbar icon (#723)
- fixed filters with hard permit can access internet (issue #689)
- fixed issue with hotkeys for switch tabs (issue #723)
- fixed rebar incrorrect resizing when dpi was changed
- fixed update installation issue (issue #1061)
- fixed notification multi-monitor support
- fixed service missing path (issue #1036)
- fixed editor can cause crash (#1071)
- fixed big memory leak (issue #1066)
- fixed net events unsubscription
- optimized listview sorting
- fixed internal bugs
- cosmetic fixes

v3.5.3 (18 October 2021)
- fixed using uninitialzed port variable in ip ranges [regression] (issue #1055)

v3.5.2 (17 October 2021)
- builded with windows 11 sdk
- fixed port can be omited when parsing ip ranges (issue #1010)
- fixed remote range replaces local range (issue #1044)
- fixed interface stuck by comctl library (issue #1009)
- fixed massive gdi handles leak
- fixed internal bugs

v3.5.1 (6 October 2021)
- optimized certificate checking
- fixed check apps for digital signatures option not works (issue #1037)
- fixed lock to avoid duplicate file information loading
- fixed crash on profile loading (issue #1033)

v3.5 (4 October 2021)
- in this release fqdn support was removed because of security issue (issues #1012)
- improved performance due caching network resolution and file information
- partially revert of windows defender power off (issue #1022)
- added ballon tip to display input limitations (issue #809)
- added compress internal profile in resources with lznt1
- fixed signatures information pointer use-after-free when dns resolver avoid arpa requests (issue #1008)
- fixed resolve network addresses can cause crash (issue #1015)
- fixed notification color issues (issue #1007)
- fixed update installation (issue #1016)
- fixed blank notification (issue #1009)
- fixed version information retrieval
- fixed workqueue environment
- fixed internal bugs

v3.4.3 (30 August 2021)
- added verify code signatures from catalog files (issue #1003)
- fixed crash at startup (issue #995)
- fixed rule reference (issue #1002)

v3.4.2 (24 August 2021)
- enable checking apps certificates by default
- show notifications immediately without waiting for busy operations to complete
- impoved listview responsiveness by using virtual listview callback
- fixed packets log displays incorrect direction (issue #945)
- fixed missed listview icons on refresh
- fixed thread environment
- fixed internal bugs
- cosmetic fixes

v3.4.1 (15 August 2021)
- fixed blocklist can does not change action status
- fixed crash at startup (issue #976)
- fixed profile import compatibility
- fixed install update

v3.4 (11 August 2021)
- added arm64 binaries (portable only)
- added protocol and host name information into network alert window (issue #843)
- added limit number of packets log entries (issue #941)
- added host resolving for packet logger listview
- added listview group for forever blocked apps
- fixed loading icons of each application freezes interface (issue #830)
- fixed log listview can have empty lines due to race condition
- fixed ui not properly display installation status (issue #962)
- fixed memory leak when loading profile (issue #888, #937)
- fixed high cpu usage for packet logger (issue #949)
- updated system rules
- updated project sdk
- fixed internal bugs

v3.3.5 (5 July 2021)
- added command line short opts
- use guid for tray icon
- updated project sdk
- fixed toolbar glitch on hidpi displays (issue #950)
- fixed multi-monitor with diffirent dpi support
- fixed installer creates unused files
- fixed internal bugs

v3.3.4 (3 June 2021)
- autoclean packets log ui when system suspended
- fixed config may not be saved when profile directory does not exist
- fixed unnecessary checking for app paths attributes
- fixed threadpool does not initialized com library
- fixed installer silent mode (issue #907)
- updated blocklist and system rules
- fixed internal bugs

v3.3.3 (29 April 2021)
- fixed listview tooltip can cause crash (issue #890)
- fixed editor does not create filters for the app
- fixed incorrect shared icons behaviour
- fixed interlocked operations logic
- fixed caching behaviour
- fixed internal bugs

v3.3.2 Beta (23 April 2021)
- added half an hour timer
- added missing spinlocks
- fixed file write operation can cause crash
- fixed path unexpand does not worked
- fixed running log viewer
- fixed little memory leak
- fixed bugs

v3.3.1 Beta (16 April 2021)
- added workaround for native paths on profile loading (issue #817)
- fixed net events does not subscribe on os version lower than 1607

v3.3 Beta (15 April 2021)
- added option for tray icon to open window in single click
- added warning message for allowing service host
- added quic service for internal rules (issue #819)
- added multiple rules at once in rules editor
- added properties dialog for apps
- added index column for logs
- added sorting for logs
- do not mark apps as unused if it applied for rules
- set limit for rules popup menu (issue #692)
- increased speed of service enumeration
- removed notification ignore button
- use threadpool for timers
- improved multi-monitor support
- improved listviews highlighting
- improved file dialogs
- fixed rule window does not display rule length limitation (issue #867)
- fixed update period timestamp does not set correctly (issue #745)
- fixed crash when user uses ipv6 range in rules (issue #822)
- fixed support filesystems with custom driver (issue #817)
- fixed blank notification window (issue #775)
- fixed dependencies load flags for win10rs1+
- fixed crash on app startup (issue #775)
- updated project sdk
- cosmetic fixes
- fixed bugs

v3.2.4 (5 September 2020)
- removed assertion from release builds (issue #764)
- removed user service instance from the list (win10+)
- fixed parsing not existing apps (issue #732, #739)
- displays incorrect name on timer expiration
- check app timer expiration on profile load
- incorrect read-only rules tooltip markup
- create filter does not report errors
- fixed checking of file attributes
- fixed parsing ip/port ranges
- cosmetic fixes
- fixed bugs

v3.2.3 (25 August 2020)
- added ncsi system rule (issue #709)
- added command line mutex checking (issue #750)
- added noficitation window redraw (issue #731)
- use logical sorting order (issue #735)
- check for provider status before create filters
- do not highlight connections in log tab
- fixed support oldest win7 versions (issue #737)
- removed listview empty markup
- cosmetic fixes
- fixed bugs

v3.2.2 (29 July 2020)
- user rules broken with 3.2.1 (issue #729)

v3.2.1 (29 July 2020)
- added Enable silent-mode when full screen app in foreground option
- added error message for createprocess failure (issue #720)
- highlighting valid connections in network tab
- changed "Disabled apps" group title into "Apps without internet access"
- skip uac warning does not worked (issue #724)
- notification window localized in english only
- revert ip version selection ipv4/ipv6 in rules editor (issue #723)
- revert expand rules in tooltip (issue #723)
- revert "recommended" tag (fix #719)
- fixed bugs

v3.2 Beta (18 July 2020)
- new rules editor
- added option "IsInternalRulesDisabled" for completely disable internal rules (issue #630)
- added temporary filtering mode whether is active until reboot (issue #576)
- added packets logging interface (issue #672)
- added tile view for listview
- added information for export profile failure (issue #707)
- improved tooltips details for apps and rules
- improved listview resizing performance
- changed "Enabled apps" group title into "Apps with internet access"
- now highlight app with user rules only when user rules group is disabled
- removed compatibility with v2 profile databases
- removed special group for rules
- removed user id from title
- fixed local network connection treats as valid network connection (issue #579)
- fixed notification window will bring focus to it's parent window (issue #668)
- fixed "/uninstall" argument does not removing filters (issue #645)
- fixed wfp engine security violation (issue #680)
- fixed support custom windows themes (issue #701)
- fixed getting uwp app paths and timestamps
- fixed dpi support (issue #693)
- fixed internal upnp rule
- fixed bugs

v3.1.2 (26 March 2020)
- reverted disable windows firewall on startup (issue #559 and #562)
- fixed application cannot be started because of continuously restart
- fixed possible duplicate apps entries with short path (issue #640)
- fixed network alert steals the focus (issue #637)
- fixed network paths parsing (issue #629)
- fixed netbios direction (issue #636)
- fixed localization (issue #607)
- fixed bugs

v3.1.1 (27 February 2020)
- fixed displaying tooltips for non-existing apps in network tab (issue #422)
- fixed wsl apps do not display names in the network tab (issue #606)
- fixed incorrect default locale name on some systems (issue #621)
- fixed process mitigation policy (win10rs2+) (issue #611)
- fixed persistent dark theme (win10rs5+) (issue #609)
- fixed update window does not close (issue #616)
- fixed log cleanup (issue #613)
- updated blocklist
- fixed bugs

v3.1 (8 February 2020)
- changed compiler build settings
- changed settings interface
- improved windows 10 1903+ suppport
- show log cleanup confirmation only when it has entries
- fixed notification tabstop processing (issue #497) by @flipkick
- fixed downloading updates progress gets stuck (issue #568)
- fixed find dialog does not worked correctly (issue #511)
- fixed drawing filename in notification (issue #595)
- fixed adjust privileges (theoretically fixes #596)
- fixed inbound direction recognition (issue #581)
- fixed search for next/active notification
- fixed notification hotkeys (issue #597)
- fixed memory duplicate allocation
- updated project sdk (routine++)
- updated ports definitions
- updated blocklist
- fixed bugs

v3.0.9 (15 November 2019)
- do not set notification to top when fullscreen apps working
- fixed disable notifications for app from notification (issue #563)
- fixed crash on system settings changing (issue #552)
- fixed winhttp encoding conversion (issue #568)
- fixed statusbar glitch (issue #569)
- updated blocklist
- fixed bugs

v3.0.8 (8 November 2019)
- removed disable windows firewall on startup (issue #554)
- updated default timer values (issue #340)
- fixed non-default system dpi (issue #544)
- fixed overwrite notifications
- fixed crash (issue #552)
- cosmetics fixes
- fixed bugs

v3.0.7 (1 November 2019)
- improved dpi support (win81+)
- fixed ended timer handling on restart
- fixed regular update message (issue #543)
- fixed components update installation
- fixed window and listview sizing
- updated project sdk
- updated blocklist
- cosmetics fixes
- fixed bugs

v3.0.6 RC (12 October 2019)
- added windows 10 1903+ support
- added csv file title (clear the log to see changes)
- disable network address resolution in network tab by default (issue #515)
- improved refreshing filters on device connecting
- improved network monitoring speed and ui
- improved notification ui
- improved dpi support (win81+)
- fixed collecting services & uwp information
- fixed device names resolution (issue #529)
- fixed toolbar gray text draw (issue #437)
- fixed network items blinking on refresh
- fixed flickering on window resizing
- fixed notification windows refresh
- fixed hostname restricted symbols
- fixed wfp security attributes
- fixed highlighting priority
- fixed memory leak
- updated system rules
- cosmetics fixes
- fixed bugs

v3.0.5 RC (6 August 2019)
- added option to block outbound connections globally
- added close connection feature (issue #506)
- added powerful blocklist configuration
- removed listen layer
- skip saving default rules configuration
- fixed connections port byte order (issue #504)
- fixed user rules ui bug (issue #500)
- fixed dropped packets log layout
- fixed internal rules editor
- fixed ipv6 rule formatting (issue #475)
- fixed connections hashing
- listview menu cosmetics
- cosmetics fixes
- fixed bugs

v3.0.4 RC (23 July 2019)
- added rules and blocklist options into main menu
- set security settings enabled by default
- set protocol names based on IEEE
- made correct backups for profile on reset
- bring notification window to top
- correct determine connections for apps
- fixed error message on new device insertion (issue #215)
- fixed missing connections (issue #423)
- fixed listview sorting does not save state sometimes
- fixed listview sorting compare checked items
- fixed uninstaller does not remove new profile
- fixed tray menu notifications cosmetics
- optimized group items counting
- optimized listview sorting
- cosmetics fixes
- fixed bugs

v3.0.3 Beta (3 July 2019)
- added option to show notification window on tray
- added highlighting for network tab
- redraw app item on connection change
- remove "beta" mark from network tab
- improved wfp transactions
- fixed ipv6 rules port parsing for system rules (issue #475)
- fixed direction case for log and notification (issue #474)
- fixed notification action adds timer (issue #472)
- fixed timer set does not work (issue #484)
- fixed notification refresh when app remove
- fixed configuration reset does not work
- fixed menu graphics (issue #473)
- fixed log stack cleanup
- updated port service names
- cosmetics fixes
- fixed bugs

v3.0.2 Beta (22 Juny 2019)
- new notification ui
- allow microsoft update & microsoft apps servers by default
- added information about ports into the log
- moved icmp4 and icmp6 rules into custom rules
- improved dropped packets log markup cosmetics
- improved network monitoring speed
- improved blocklist configuration
- fixed saving special rules for uwp apps and services (issue #454)
- fixed tray icon dissapear sometimes (issue #450)
- fixed profile import does not work (issue #445)
- fixed semicolons in app paths (issue #462)
- fixed windows 20h1 uwp apps loading crash
- fixed parsing rules with "*"
- updated locales
- cosmetic fixes
- fixed bugs

v3.0.1 Beta (22 May 2019)
- new profile structure
- added checking for correct xml file format before import
- added service name resolution for connections
- added Reverse DNS Lookup for IP
- more flexible autoresizing for listview
- fixed column size initialization (issue #426)
- fixed apps list sorting by date (issue #412)
- fixed status refreshing on timer deletion
- fixed table view for rules (issue #438)
- fixed connections recognition condition
- fixed highlighting (issue #408)
- fixed timers sets (issue #424)
- fixed services icons
- updated locales
- cosmetic fixes
- fixed bugs

v3.0 Beta (7 May 2019)
- dropped windows vista support
- improved dpi support (ui)
- moved rules list into main window
- added services and uwp apps tab
- added network connections monitor
- fixed displaying notifications when it's disabled
- fixed log file write through (issue #369)
- fixed notifications safety timeout
- fixed possible race conditions
- fixed services path retrieve
- fixed notifications cleanup
- improved listview resizing
- improved listview sorting
- removed blacklist mode
- cosmetic fixes
- fixed bugs
