# TAP test of the NT service code in main.c: registration, the registered
# image path and start type, then the start/stop/uninstall lifecycle.
#
# Windows only, needs Administrator, and it writes to the service control
# manager, so it is a CI job and an nmake check-service target rather than
# part of check. Every assertion is made against the SCM through an
# independent channel, never against askl.exe's own exit status, because the
# binary reporting success is exactly what is under test.

$ErrorActionPreference = 'Continue'

$svc = 'ASKL'
$exe = Join-Path (Get-Location) 'askl.exe'
$n = 0
$failed = 0

function Emit-Tap([bool] $cond, [string] $name, [string] $directive = '') {
    $script:n ++
    if ($cond) { $line = "ok $script:n - $name" }
    else       { $line = "not ok $script:n - $name" }
    if ($directive) { $line = "$line # $directive" }
    Write-Output $line
    if (-not $cond -and -not "$directive".StartsWith('TODO')) {
        $script:failed ++
    }
}

function Emit-Diag([string] $m) { Write-Output "#   $m" }

function Get-Svc {
    Get-CimInstance Win32_Service -Filter "Name='$svc'" `
        -ErrorAction SilentlyContinue
}

function Invoke-Askl([string] $flag) {
    $out = & $exe $flag 2>&1 | Out-String
    Emit-Diag "askl.exe $flag -> exit $LASTEXITCODE"
    foreach ($l in ($out -split "`r?`n")) {
        if ($l.Trim()) { Emit-Diag $l.Trim() }
    }
}

function Wait-State([string] $want, [int] $seconds) {
    for ($i = 0; $i -lt ($seconds * 4); $i ++) {
        $s = Get-Svc
        if (-not $s) { return $null }
        if ($s.State -eq $want) { return $s.State }
        Start-Sleep -Milliseconds 250
    }
    $s = Get-Svc
    if ($s) { return $s.State } else { return $null }
}

function Remove-Leftover {
    if (Get-Svc) {
        Emit-Diag "cleanup: a $svc service is registered, removing it"
        & sc.exe stop   $svc 2>&1 | Out-Null
        Start-Sleep -Milliseconds 500
        & sc.exe delete $svc 2>&1 | Out-Null
    }
}

Write-Output '1..8'

Emit-Tap (Test-Path $exe) 'service.binary_built'

$admin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)

$skip = ''
if (-not $admin) { $skip = 'SKIP not Administrator' }
Emit-Tap $admin 'service.elevated' $skip

if (-not (Test-Path $exe) -or -not $admin) {
    $why = 'SKIP askl.exe was not built'
    if (-not $admin) { $why = 'SKIP needs Administrator' }
    foreach ($t in @('service.install_registers',
                     'service.image_path',
                     'service.start_type',
                     'service.start_runs',
                     'service.stop_keeps_registration',
                     'service.uninstall_removes')) {
        Emit-Tap $true $t $why
    }
    exit 0
}

Remove-Leftover

try {
    # ---- registration ----------------------------------------------------
    Invoke-Askl '/install'
    $s = Get-Svc
    Emit-Tap ($null -ne $s) 'service.install_registers'

    # CreateService() is handed GetModuleFileName() unquoted, so a path with
    # a space would register a different image than it names
    $path = ''
    if ($s) { $path = $s.PathName }
    Emit-Diag "PathName = [$path]"
    Emit-Tap ($path -eq $exe -or $path -eq "`"$exe`"") 'service.image_path'

    $mode = ''
    if ($s) { $mode = $s.StartMode }
    Emit-Diag "StartMode = [$mode]"
    Emit-Tap ($mode -eq 'Auto') 'service.start_type'

    # ---- lifecycle -------------------------------------------------------
    Invoke-Askl '/start'
    $state = Wait-State 'Running' 25
    Emit-Diag "state after /start = [$state]"
    Emit-Tap ($state -eq 'Running') 'service.start_runs'

    # /stop is wired to _service_uninstall(), not _service_stop(), which is
    # dead code: on a running service it fails, and on a stopped one it
    # deletes the registration while printing that it stopped
    Invoke-Askl '/stop'
    $state = Wait-State 'Stopped' 25
    $still = $null -ne (Get-Svc)
    Emit-Diag "state after /stop = [$state], still registered = $still"
    Emit-Tap ($still -and $state -eq 'Stopped') `
        'service.stop_keeps_registration' `
        'TODO /stop calls _service_uninstall(), _service_stop() is dead code'

    # ---- removal ---------------------------------------------------------
    if (Get-Svc) {
        Invoke-Askl '/uninstall'
    } else {
        Emit-Diag '/stop already removed the registration'
    }
    Emit-Tap ($null -eq (Get-Svc)) 'service.uninstall_removes'
}
finally {
    Remove-Leftover
}

Write-Output "# $($n - $failed) passed, $failed failed of $n"
if ($failed -gt 0) { exit 1 } else { exit 0 }
