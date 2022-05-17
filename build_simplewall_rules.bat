@echo off

cd ..\builder
call build_simplewall_rules %~dp0bin\profile_internal.sp

pause
