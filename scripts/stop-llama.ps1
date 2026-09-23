# Stop only the model process started by this project's launcher, after checking its identity.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent # Repository containing the saved process record.
$recordPath = Join-Path $projectRoot 'tmp/llama-process.json' # Launcher-owned PID and executable path.
if (-not (Test-Path -LiteralPath $recordPath)) { Write-Output 'No recorded model process.'; exit 0 }
$record = Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json # Exact process ownership data.
$modelProcess = Get-Process -Id $record.id -ErrorAction SilentlyContinue # Detect PID reuse before stopping.
if ($modelProcess -and $modelProcess.Path -eq $record.path) { Stop-Process -Id $record.id; Write-Output 'Stopped project llama.cpp server.' }
Remove-Item -LiteralPath $recordPath
