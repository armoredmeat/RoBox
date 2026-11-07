# Builds RoBox (Release), runs every test, then makes the installer in dist\.
# Usage (from the RoBox folder):  powershell -ExecutionPolicy Bypass -File Installer\build-installer.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup" }

# The version comes from CMakeLists.txt, so there's only one place to change it.
$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(RoBox VERSION ([0-9.]+)').Matches[0].Groups[1].Value
Write-Host "Building RoBox $version" -ForegroundColor Cyan

& $cmake --build build --config Release --target RoBox_VST3 RoBox_Standalone RoBoxTests RoBoxHostTest -- -m
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host "Running tests" -ForegroundColor Cyan
& .\build\RoBoxTests_artefacts\Release\RoBoxTests.exe | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { throw "Unit tests failed: not making an installer" }
& .\build\RoBoxHostTest_artefacts\Release\RoBoxHostTest.exe "build\RoBox_artefacts\Release\VST3\RoBox.vst3" | Select-Object -Last 1
if ($LASTEXITCODE -ne 0) { throw "VST3 host test failed: not making an installer" }

Write-Host "Making the installer" -ForegroundColor Cyan
& $iscc /Q "/DAppVersion=$version" Installer\RoBox.iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }

$setup = Get-Item "dist\RoBox-$version-Setup.exe"
Write-Host ("Done: {0} ({1:N1} MB)" -f $setup.FullName, ($setup.Length / 1MB)) -ForegroundColor Green
