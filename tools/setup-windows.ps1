# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallDirectory,
    [switch]$WithoutDictionary
)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$application = [IO.Path]::GetFullPath($InstallDirectory)
$data = Join-Path $application 'data'
$runtime = Join-Path $data 'argos-env'
$scripts = Join-Path $runtime 'Scripts'
$python = Join-Path $scripts 'python.exe'
$bootstrap = Join-Path $data 'bootstrap'
$temporary = Join-Path $data 'temporary'
New-Item -ItemType Directory -Force -Path $data, $bootstrap, $temporary | Out-Null
$env:TEMP = $temporary
$env:TMP = $temporary
$env:PIP_CACHE_DIR = Join-Path $data 'pip-cache'
$env:PYTHONIOENCODING = 'utf-8'
$records = @()
function Get-SetupDownload([string]$Url, [string]$Destination) {
    Write-Host "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing -Headers @{ 'User-Agent' = 'HoverTranslate/0.6.0' }
    [PSCustomObject]@{ url = $Url; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash.ToLowerInvariant() }
}
if (!(Test-Path -LiteralPath $python)) {
    # Embedded CPython keeps the runtime independent of another Python installation.
    $archive = Join-Path $bootstrap 'python-3.10.11-embed-amd64.zip'
    $records += Get-SetupDownload 'https://www.python.org/ftp/python/3.10.11/python-3.10.11-embed-amd64.zip' $archive
    New-Item -ItemType Directory -Force -Path $scripts | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $scripts -Force
    $pathFile = Join-Path $scripts 'python310._pth'
    @('python310.zip', '.', 'Lib\site-packages', 'import site') | Set-Content -LiteralPath $pathFile -Encoding ASCII
}
& $python -c 'import sys; assert sys.version_info[:2] == (3,10), sys.version; assert sys.maxsize > 2**32'
if ($LASTEXITCODE -ne 0) { throw 'The private Python runtime did not start.' }
& $python -c "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('pip') else 1)"
if ($LASTEXITCODE -ne 0) {
    $getPip = Join-Path $bootstrap 'get-pip.py'
    $records += Get-SetupDownload 'https://bootstrap.pypa.io/get-pip.py' $getPip
    & $python $getPip --disable-pip-version-check
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare pip in the private runtime.' }
}
if ($records.Count -gt 0) {
    $manifest = Join-Path $bootstrap 'download-manifest.json'
    $previous = @()
    if (Test-Path -LiteralPath $manifest) { $previous = @(Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json) }
    ConvertTo-Json -InputObject @($previous + $records) -Depth 5 | Set-Content -LiteralPath $manifest -Encoding UTF8
}
$helper = Join-Path $PSScriptRoot 'setup_offline.py'
if (!(Test-Path -LiteralPath $helper)) { $helper = Join-Path $PSScriptRoot '..\offline\setup_offline.py' }
if (!(Test-Path -LiteralPath $helper)) { throw 'setup_offline.py is missing beside the setup script.' }
$arguments = @('-u', $helper, '--data-dir', $data)
if ($WithoutDictionary) { $arguments += '--without-dictionary' }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Offline setup failed. See the preceding download or runtime error.' }
Write-Output $python
