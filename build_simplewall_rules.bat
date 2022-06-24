@echo off

cd ..\builder
call build_simplewall_rules update %~dp0bin\profile_internal.sp %~dp0bin\profile_internal.sp

pause
