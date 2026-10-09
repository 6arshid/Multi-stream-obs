[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
python -m pip install aqtinstall==3.3.0
if ($LASTEXITCODE -ne 0) { throw 'Could not install aqtinstall' }
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --archives qtbase -O (Join-Path $ProjectRoot '.deps/qt-test-sdk')
if ($LASTEXITCODE -ne 0) { throw 'Could not install the Qt Test SDK' }
