# Removes the plugin DLL from CLO's plug-in folders. Close CLO first if the
# listener was started this session (the DLL is pinned while it runs).
$ErrorActionPreference = "Stop"

$paths = @(
    "C:\Users\Public\Documents\CLO\Assets\Preferences\API_Plug_in\clo_mcp_plugin.dll",
    "C:\Users\Public\Documents\CLO\Plugins\clo_mcp_plugin.dll",
    "C:\Users\Public\Documents\CLO\Plugins\CLOAPIInterface.dll"
)

$removed = 0
foreach ($p in $paths) {
    if (Test-Path $p) {
        try {
            Remove-Item $p -Force -Confirm:$false -ErrorAction Stop
            Write-Host "[uninstall] removed $p"
            $removed++
        } catch {
            throw "Cannot remove $p - close CLO and re-run."
        }
    }
}
if ($removed -eq 0) { Write-Host "[uninstall] nothing to remove - plugin was not installed" }
else { Write-Host "[uninstall] done ($removed file(s))" }
