param([string]$RomDirectory = (Join-Path $PSScriptRoot 'roms'))
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
if(!(Test-Path -LiteralPath (Join-Path $taskRoot 'vcop2.exe'))) {$taskRoot=Split-Path -Parent $PSScriptRoot}
$taskRom=[IO.Path]::GetFullPath($RomDirectory)
foreach($taskFile in @('vcop2.zip')) {
    if(!(Test-Path -LiteralPath (Join-Path $taskRom $taskFile) -PathType Leaf)) {throw "Missing owned game file: $taskRom\$taskFile"}
}
Push-Location -LiteralPath $taskRoot
try {
    & (Join-Path $taskRoot 'vcop2.exe') -rompath $taskRom -verifyroms vcop2
    if($LASTEXITCODE){throw 'MAME rejected the game data. Use the complete Virtua Cop 2 Model 2 arcade set plus the 315-5649 I/O ROM.'}
    $taskData=Join-Path $taskRoot 'userdata'
    foreach($taskDirectory in @('nvram','cfg','diff','state','snap')) {New-Item -ItemType Directory -Path (Join-Path $taskData $taskDirectory) -Force | Out-Null}
    $env:VCOP2_VR='1'
    $env:VCOP2_XR_LIBRARY=Join-Path $taskRoot 'vcop2_xr.dll'
    $env:VCOP2_ASSETS=Join-Path $taskRoot 'assets'
    $env:VCOP2_SETTINGS=Join-Path $taskData 'settings.ini'
    $env:VCOP2_XR_LOG=Join-Path $taskData 'vcop2-xr.log'
    & (Join-Path $taskRoot 'vcop2.exe') vcop2 -rompath $taskRom -nvram_directory (Join-Path $taskData 'nvram') `
        -cfg_directory (Join-Path $taskData 'cfg') -diff_directory (Join-Path $taskData 'diff') `
        -state_directory (Join-Path $taskData 'state') -snapshot_directory (Join-Path $taskData 'snap') `
        -video none -sound auto -skip_gameinfo -noreadconfig
    if(Test-Path -LiteralPath $env:VCOP2_XR_LOG) {Get-Content -LiteralPath $env:VCOP2_XR_LOG -Tail 15}
} finally {Pop-Location}
