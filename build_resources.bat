@echo off

php "..\builder\make_resource.php" ".\src\resource.hpp"
php "..\builder\make_locale.php" "simplewall" "simplewall" ".\bin\i18n" ".\src\resource.hpp" ".\src\resource.rc" ".\bin\simplewall.lng"
copy /y ".\bin\simplewall.lng" ".\bin\32\simplewall.lng"
copy /y ".\bin\simplewall.lng" ".\bin\64\simplewall.lng"

pause
