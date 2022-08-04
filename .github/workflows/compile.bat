@echo on

cd %GITHUB_WORKSPACE%

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\amd64\msbuild.exe" -m /property:Configuration=Release /property:Platform=arm64
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\amd64\msbuild.exe" -m /property:Configuration=Release /property:Platform=x64
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\amd64\msbuild.exe" -m /property:Configuration=Release /property:Platform=x86
