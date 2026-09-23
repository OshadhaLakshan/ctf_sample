param(
    # Text GGUF downloaded separately from the llama.cpp runtime.
    [string]$ModelPath = '.\models\gemma-4-E2B-it-Q4_0.gguf',
    # Optional explicit runtime location; otherwise discover PATH and winget.
    [string]$ServerPath = '',
    # Dedicated local inference port, separate from Crow and Vite.
    [int]$Port = 8081
)
$ErrorActionPreference = 'Stop'
# Resolve all local artifacts relative to this repository.
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not [IO.Path]::IsPathRooted($ModelPath)) { $ModelPath = Join-Path $projectRoot $ModelPath }
$resolvedModel = (Resolve-Path -LiteralPath $ModelPath).Path
if ($Port -lt 1024 -or $Port -gt 65535 -or $Port -in @(8080, 3000)) { throw 'Choose an inference port between 1024 and 65535, separate from the app.' }
if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) { throw "Port $Port is already occupied. Check the existing server before starting another." }
if (-not $ServerPath) {
    # A recently installed winget package may not yet be in this terminal's PATH.
    $runtimeCommand = Get-Command llama-server.exe -ErrorAction SilentlyContinue
    if ($runtimeCommand) { $ServerPath = $runtimeCommand.Source }
    else {
        $packageRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
        $runtimeMatch = Get-ChildItem -LiteralPath $packageRoot -Directory -Filter 'ggml.llamacpp*' -ErrorAction SilentlyContinue | ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Filter 'llama-server.exe' -Recurse } | Select-Object -First 1
        if ($runtimeMatch) { $ServerPath = $runtimeMatch.FullName }
    }
}
if (-not $ServerPath) { throw 'llama-server.exe was not found. Install llama.cpp with winget or provide -ServerPath.' }
$resolvedServer = (Resolve-Path -LiteralPath $ServerPath).Path
# Persistent log files support troubleshooting even though the helper window stays hidden.
$logRoot = Join-Path $projectRoot 'tmp'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
# Native argument quoting protects spaces; Windows filenames cannot contain a double quote.
$serverArguments = '-m "' + $resolvedModel + '" --host 127.0.0.1 --port ' + $Port + ' --ctx-size 8192 --parallel 1 --jinja'
$serverProcess = Start-Process -FilePath $resolvedServer -ArgumentList $serverArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logRoot 'llama.log') -RedirectStandardError (Join-Path $logRoot 'llama-error.log')
@{ id = $serverProcess.Id; path = $resolvedServer; model = $resolvedModel; port = $Port } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logRoot 'llama-process.json')
Write-Output "llama.cpp started on http://127.0.0.1:$Port (PID $($serverProcess.Id)). Model loading may take a minute. Logs: tmp/llama-error.log"
