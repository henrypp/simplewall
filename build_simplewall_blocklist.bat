@echo off

cd ..\builder
call build_simplewall_blocklist %~dp0bin\profile_internal.sp

pause
