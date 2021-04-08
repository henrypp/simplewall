@echo off

cd ..\builder
build_locale simplewall simplewall

copy /y ".\bin\simplewall.lng" ".\bin\32\simplewall.lng"
copy /y ".\bin\simplewall.lng" ".\bin\64\simplewall.lng"

pause
