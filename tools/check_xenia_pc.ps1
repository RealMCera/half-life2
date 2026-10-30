# Half-Life 1 Xbox 360 remaster: read-only Windows Xenia/SDK readiness check.
# Does not install software, modify emulator settings, or launch files.
[CmdletBinding()]
param(
    [string]$XeniaPath,
    [string]$XexPath,
    [string]$ReportPath = (Join-Path (Get-Location).Path 'HL1_Xenia_PC_Check.txt')
)

$ErrorActionPreference = 'Stop'
$results = [System.Collections.Generic.List[string]]::new()
function Note([string]$message) {
    $results.Add($message)
    Write-Host $message
}

Note 'HALF-LIFE 1 REMASTER - XENIA PC READINESS'
Note ('Checked at: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Note 'Read-only diagnostic: no downloads, installs, or game launches.'
Note ''

# Only examine familiar user folders for executables; avoid a full-drive scan.
$candidates = [System.Collections.Generic.List[string]]::new()
if ($XeniaPath) { $candidates.Add($XeniaPath) }
foreach ($commandName in @('xenia_canary.exe', 'xenia.exe')) {
    $exe = Get-Command $commandName -ErrorAction SilentlyContinue
    if ($exe -and $exe.Source) { $candidates.Add($exe.Source) }
}
foreach ($root in @((Join-Path $env:USERPROFILE 'Downloads'), (Join-Path $env:USERPROFILE 'Desktop'), (Join-Path $env:USERPROFILE 'Documents'))) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
    Get-ChildItem -LiteralPath $root -File -Filter '*xenia*.exe' -Recurse -Depth 2 -ErrorAction SilentlyContinue |
        Select-Object -First 10 |
        ForEach-Object { $candidates.Add($_.FullName) }
}
$installedXenia = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -Unique
if ($installedXenia) {
    Note ('[OK] Xenia executables found: ' + $installedXenia.Count)
    foreach ($path in $installedXenia) { Note ('     ' + $path) }
} else {
    Note '[CHECK] No Xenia executable found in shallow Downloads/Desktop/Documents scan.'
    Note '        Rerun with -XeniaPath "C:\path\to\xenia_canary.exe" if installed elsewhere.'
}

foreach ($name in @('git.exe', 'py.exe', 'python.exe', 'wsl.exe')) {
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($command) { Note ('[OK] Found ' + $name) }
    else { Note ('[CHECK] Missing or not on PATH: ' + $name) }
}

$wsl = Get-Command 'wsl.exe' -ErrorAction SilentlyContinue
if ($wsl) {
    try {
        $wslOutput = @(& wsl.exe --list --verbose 2>&1)
        $wslCode = $LASTEXITCODE
        if ($wslCode -eq 0) {
            Note '[INFO] WSL distributions:'
            foreach ($line in $wslOutput) {
                $readable = ([string]$line -replace '\x00', '').Trim()
                if ($readable) { Note ('     ' + $readable) }
            }
            if (($wslOutput -join ' ') -notmatch 'Debian') {
                Note '[NEXT] xsdk recommends Debian WSL2; install Debian if needed.'
            }
        } else {
            Note '[CHECK] WSL is installed but distribution listing did not succeed.'
        }
    } catch {
        Note ('[CHECK] WSL query error: ' + $_.Exception.Message)
    }
}

try {
    $freeGB = [math]::Round((Get-PSDrive -Name $env:SystemDrive.TrimEnd(':') -ErrorAction Stop).Free / 1GB, 1)
    Note ('[INFO] Free disk space on system drive: ' + $freeGB + ' GB')
    if ($freeGB -lt 10) { Note '[CHECK] xsdk documents needing approximately 10 GB free for its WSL toolchain build.' }
} catch {
    Note '[INFO] Disk space check unavailable.'
}

if ($XexPath) {
    try {
        $xex = Get-Item -LiteralPath $XexPath -ErrorAction Stop
        if ($xex -isnot [System.IO.FileInfo]) { throw 'Not a file' }
        $stream = [System.IO.File]::OpenRead($xex.FullName)
        try {
            $header = New-Object byte[] 4
            $read = $stream.Read($header, 0, 4)
        } finally { $stream.Dispose() }
        if ($read -eq 4 -and [System.Text.Encoding]::ASCII.GetString($header) -eq 'XEX2') {
            Note '[OK] Candidate Xbox 360 file has XEX2 magic (header only; not a validity/boot test).'
        } else {
            Note '[CHECK] Candidate executable is not XEX2. Original Xbox XBE files cannot run in Xenia.'
        }
    } catch {
        Note ('[CHECK] XEX check failed: ' + $_.Exception.Message)
    }
} else {
    Note '[INFO] No XEX supplied. This check does not include or build an executable.'
}

Note ''
Note 'NEXT STEPS:'
Note '1. Get Windows Xenia Canary ready (already installed is fine).'
Note '2. If Debian WSL2 or build dependencies are missing, follow xsdk Getting Started.'
Note '3. Compile the xsdk hello.xex sample, test its startup log in Xenia Canary.'
Note '4. Xenia may not support xsdk display-controller functions; a log-only smoke check still matters.'
Note '5. Keep the Xbox Series S test for later, using the exact same XEX when supported.'
Note 'Docs: https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md'
Note ''

try {
    $directory = Split-Path -Path $ReportPath -Parent
    if ($directory -and -not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $results | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    Write-Host ('Report saved: ' + (Resolve-Path -LiteralPath $ReportPath).Path)
} catch {
    Write-Warning ('Could not write readiness report: ' + $_.Exception.Message)
}
