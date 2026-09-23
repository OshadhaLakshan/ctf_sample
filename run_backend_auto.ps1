while ($true) {
    $cmake = Get-Process cmake -ErrorAction SilentlyContinue
    $mingw = Get-Process mingw32-make -ErrorAction SilentlyContinue
    if (!$cmake -and !$mingw) {
        break
    }
    Start-Sleep -Seconds 2
}
Write-Host "Build finished. Starting backend..."
./build/rowdogg.exe
