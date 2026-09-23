# Stop only process IDs recorded by this project's launcher after checking executable identity.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent # Repository root for the launcher state file.
$pidFile = Join-Path $projectRoot 'tmp/services.json' # Project-owned service records.
if (-not (Test-Path -LiteralPath $pidFile)) { Write-Output 'No recorded R0WD0GG services.'; exit 0 }
$records = Get-Content -LiteralPath $pidFile -Raw | ConvertFrom-Json # Saved IDs and expected executable paths.
foreach ($record in $records) {
    $process = Get-CimInstance Win32_Process -Filter "ProcessId=$($record.id)" -ErrorAction SilentlyContinue # CIM retains the executable path when Get-Process.Path is unavailable.
    if ($process -and $process.ExecutablePath -eq $record.path) {
        Stop-Process -Id $record.id -Force
        Wait-Process -Id $record.id -Timeout 10 -ErrorAction SilentlyContinue
        Write-Output "Stopped $($record.name)."
    }
}
Remove-Item -LiteralPath $pidFile
