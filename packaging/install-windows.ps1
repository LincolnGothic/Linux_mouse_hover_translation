# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([string]$InstallDirectory = 'D:\software\HoverTranslate\app-0.7.0',
      [switch]$SkipOfflineSetup)
$ErrorActionPreference = 'Stop'
$source=[IO.Path]::GetFullPath($PSScriptRoot)
$target=[IO.Path]::GetFullPath($InstallDirectory)
if ($source.TrimEnd('\') -ne $target.TrimEnd('\')) {
    if (Test-Path -LiteralPath $target) { throw "Installation target already exists: $target. Choose a new empty folder." }
    New-Item -ItemType Directory -Path $target | Out-Null
    Get-ChildItem -LiteralPath $source -Force | Copy-Item -Destination $target -Recurse
}
if (!(Test-Path -LiteralPath (Join-Path $target 'hover-translate.exe'))) { throw 'The Windows application is missing.' }
if (!$SkipOfflineSetup) {
    & (Join-Path $target 'share\hover-translate\setup-windows.ps1') -InstallDirectory $target
}
$desktop=[Environment]::GetFolderPath('Desktop')
$shortcutPath=Join-Path $desktop 'Hover Translate.lnk'
if (Test-Path -LiteralPath $shortcutPath) { $shortcutPath=Join-Path $desktop 'Hover Translate 0.7.0.lnk' }
if (Test-Path -LiteralPath $shortcutPath) { throw "Shortcut already exists: $shortcutPath" }
$shell=New-Object -ComObject WScript.Shell
$shortcut=$shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath=Join-Path $target 'hover-translate.exe'
$shortcut.WorkingDirectory=$target
$shortcut.Description='English and Chinese hover translation for Windows'
$shortcut.Save()
Write-Output "Installed Hover Translate in $target"
Write-Output "Launch shortcut: $shortcutPath"
