# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$MsysRoot,
      [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-windows'))
$ErrorActionPreference = 'Stop'
$env:PYTHONIOENCODING = 'utf-8'
$prefix = Join-Path ([IO.Path]::GetFullPath($MsysRoot)) 'ucrt64'
$env:PATH = "$prefix\bin;$env:PATH"
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [IO.Path]::GetFullPath($BuildDirectory)
& "$prefix\bin\cmake.exe" -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DCMAKE_PREFIX_PATH=$prefix"
if ($LASTEXITCODE -ne 0) { throw 'Windows configuration failed.' }
& "$prefix\bin\cmake.exe" --build $build --parallel 3
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed.' }
$env:TESSDATA_PREFIX = Join-Path $prefix 'share\tessdata'
New-Item -ItemType Directory -Force -Path (Join-Path $build 'tests\tessdata') | Out-Null
Copy-Item -LiteralPath (Join-Path $env:TESSDATA_PREFIX 'eng.traineddata'),(Join-Path $env:TESSDATA_PREFIX 'chi_sim.traineddata') -Destination (Join-Path $build 'tests\tessdata')
$fontDirectory = Join-Path $build 'test-assets'
New-Item -ItemType Directory -Force -Path $fontDirectory | Out-Null
$env:HOVER_TEST_FONT = Join-Path $fontDirectory 'NotoSansCJKsc-Regular.otf'
if (!(Test-Path -LiteralPath $env:HOVER_TEST_FONT)) {
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf' -OutFile $env:HOVER_TEST_FONT -UseBasicParsing
}
& "$prefix\bin\ctest.exe" --test-dir $build --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Windows tests failed.' }
