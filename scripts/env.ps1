$root = Split-Path -Parent $PSScriptRoot
$env:VITASDK = Join-Path $root "tools\vitasdk\vitasdk"
$env:PATH = "$env:VITASDK\bin;$(Join-Path $root 'tools\ninja');$env:PATH"

Write-Host "VITASDK=$env:VITASDK"
arm-vita-eabi-gcc --version | Select-Object -First 1
ninja --version

