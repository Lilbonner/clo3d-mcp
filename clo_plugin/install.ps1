# Installs the built plugin DLL into CLO's plug-in folder.
#
#   .\install.ps1                 # copy now (fails if CLO holds the DLL pinned)
#   .\install.ps1 -WaitForClose   # keep retrying until CLO exits (up to 10 min)
#
# CLO loads plugin DLLs from Assets\Preferences\API_Plug_in at every menu click,
# so a fresh DLL is picked up without restarting CLO - UNLESS the running
# listener has pinned the module; then close CLO first (or use -WaitForClose).
param(
    [switch]$WaitForClose
)
$ErrorActionPreference = "Stop"

$dll = Join-Path $PSScriptRoot "build-msvc\Release\clo_mcp_plugin.dll"
if (-not (Test-Path $dll)) { throw "Plugin not built yet - run .\build.ps1 first" }

$dest = "C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in"
New-Item -ItemType Directory -Force $dest | Out-Null
$target = Join-Path $dest "clo_mcp_plugin.dll"

$clo = Get-Process -Name "CLO*" -ErrorAction SilentlyContinue
if ($clo) {
    Write-Host "[install] note: CLO is running (PID $($clo[0].Id)). The copy fails only if the listener is started (DLL pinned)."
}

$deadline = (Get-Date).AddMinutes(10)
while ($true) {
    try {
        Copy-Item $dll $target -Force -ErrorAction Stop
        break
    } catch {
        if (-not $WaitForClose) {
            throw ("Copy failed - the DLL is pinned by a running CLO. Close CLO and re-run, " +
                   "or use: .\install.ps1 -WaitForClose")
        }
        if ((Get-Date) -gt $deadline) { throw "Timed out waiting for CLO to release the DLL." }
        Start-Sleep -Seconds 2
    }
}

Write-Host "[install] OK -> $target"
Write-Host "[install] in CLO: Settings -> Plug-in -> 'MCP Listener (start / stop)'"
Write-Host "[install] (restart CLO first if it was already open with the listener started)"
