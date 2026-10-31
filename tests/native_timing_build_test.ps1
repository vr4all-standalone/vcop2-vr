$ErrorActionPreference='Stop'
$taskScript=Join-Path $PSScriptRoot '../tools/build-engine.ps1'
$taskTokens=$null;$taskErrors=$null
$taskAst=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $taskScript),[ref]$taskTokens,[ref]$taskErrors)
if($taskErrors.Count){throw 'Build script has PowerShell parse errors.'}
if(!($taskAst.ParamBlock.Parameters.Name.VariablePath.UserPath -contains 'NativeTiming')){throw 'Diagnostic build switch is missing.'}
$taskSource=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $taskScript))
$taskBegin=$taskSource.IndexOf('$taskTimingConfig =')
$taskEnd=$taskSource.IndexOf('# Verify full cumulative patch states',$taskBegin)
if($taskBegin -lt 0 -or $taskEnd -lt 0){throw 'Timing config generator was not found before the compiler.'}
$taskGenerate=[ScriptBlock]::Create($taskSource.Substring($taskBegin,$taskEnd-$taskBegin))
$taskNative=Join-Path ([IO.Path]::GetTempPath()) ('vcop2-native-timing-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path $taskNative | Out-Null
try {
    $NativeTiming=$false
    . $taskGenerate
    $taskFile=Join-Path $taskNative 'native_timing_config.h'
    if(!([IO.File]::ReadAllText($taskFile).Contains('#define VCOP2_NATIVE_TIMING_AVAILABLE 0'))){throw 'Default production config did not disable timing.'}
    $taskBytes=[IO.File]::ReadAllBytes($taskFile)
    if($taskBytes[0] -ne 47){throw 'Generated config must use UTF8 without BOM.'}
    [IO.File]::SetLastWriteTimeUtc($taskFile,[datetime]'2020-01-01T00:00:00Z')
    . $taskGenerate
    if([IO.File]::GetLastWriteTimeUtc($taskFile).Year -ne 2020){throw 'An unchanged config was rewritten.'}
    $NativeTiming=$true
    . $taskGenerate
    if(!([IO.File]::ReadAllText($taskFile).Contains('#define VCOP2_NATIVE_TIMING_AVAILABLE 1'))){throw 'Diagnostic switch did not enable timing.'}
    [IO.File]::SetLastWriteTimeUtc($taskFile,[datetime]'2020-01-01T00:00:00Z')
    . $taskGenerate
    if([IO.File]::GetLastWriteTimeUtc($taskFile).Year -ne 2020){throw 'Unchanged diagnostic config was rewritten.'}
    $NativeTiming=$false
    . $taskGenerate
    if(!([IO.File]::ReadAllText($taskFile).Contains('#define VCOP2_NATIVE_TIMING_AVAILABLE 0'))){throw 'Returning to production did not disable timing.'}
    Write-Output 'Native timing generated config, production default and stable timestamps passed'
} finally {
    Remove-Item -LiteralPath (Join-Path $taskNative 'native_timing_config.h') -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $taskNative -ErrorAction SilentlyContinue
}
