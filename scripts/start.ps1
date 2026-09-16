param([switch]$WithLab, [switch]$Dev)
# Stop on errors so a failed build is never presented as a running service.
$ErrorActionPreference = 'Stop'
# Resolve the repository root independently of the caller's current directory.
$projectRoot = Split-Path $PSScriptRoot -Parent
# Persist process IDs only for services launched by this script.
$pidFile = Join-Path $projectRoot 'tmp/services.json'
# Platform-specific compiled API executable.
$backendPath = Join-Path $projectRoot 'build/rowdogg.exe'
if (-not (Test-Path -LiteralPath $backendPath)) { throw 'Build the backend first. See README.md.' }
if (-not $Dev -and -not (Test-Path -LiteralPath (Join-Path $projectRoot 'frontend/dist/index.html'))) { throw 'Run npm run build in frontend first.' }
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'tmp') | Out-Null
# Reject occupied application ports rather than taking over an unrelated service.
$ports = @(8080)
if ($WithLab) { $ports += 8090 }
if ($Dev) { $ports += 3000 }
foreach ($port in $ports) {
    if (Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue) { throw "Port $port is already in use. Stop the existing service first." }
}
# Each process record includes its executable path to support safe subsequent shutdown.
$records = @()
try {
    $backend = Start-Process -FilePath $backendPath -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $projectRoot 'tmp/backend.log') -RedirectStandardError (Join-Path $projectRoot 'tmp/backend-error.log')
    $records += @{ id = $backend.Id; path = $backendPath; name = 'backend' }
    if ($WithLab) {
        $pythonPath = (Get-Command python -ErrorAction Stop).Source
        $lab = Start-Process -FilePath $pythonPath -ArgumentList 'scripts/lab_fixture.py' -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $projectRoot 'tmp/lab.log') -RedirectStandardError (Join-Path $projectRoot 'tmp/lab-error.log')
        $records += @{ id = $lab.Id; path = $pythonPath; name = 'lab' }
    }
    if ($Dev) {
        $nodePath = (Get-Command node -ErrorAction Stop).Source
        $frontend = Start-Process -FilePath $nodePath -ArgumentList 'node_modules/vite/bin/vite.js --host 127.0.0.1 --port 3000 --strictPort' -WorkingDirectory (Join-Path $projectRoot 'frontend') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $projectRoot 'tmp/frontend.log') -RedirectStandardError (Join-Path $projectRoot 'tmp/frontend-error.log')
        $records += @{ id = $frontend.Id; path = $nodePath; name = 'frontend' }
    }
    $records | ConvertTo-Json | Set-Content -LiteralPath $pidFile
    Write-Output "R0WD0GG started: http://127.0.0.1:$(if ($Dev) { 3000 } else { 8080 })"
    Write-Output 'Stop these services with ./scripts/stop.ps1. Logs are in tmp/.'
} catch {
    foreach ($record in $records) { Stop-Process -Id $record.id -ErrorAction SilentlyContinue }
    throw
}
