# shh-nx (2026-11-11, from mgsv-nx and sa2-nx's tools/win/pgo_merge.ps1): profile-guided optimisation, step 2 of 3.
# Step 1 is the instrumented NRO (tools\build_switch.bat <nvk_sdk> pgo-generate), played on the console: it writes
# pgo/shh_pgo_<N>.stream next to the NRO (one per session, app/src/shh_pgo.cpp). Copy that pgo folder to the PC and
# run this: every session's counts are added into the .gcda files of the profile folder (gcov-tool merge-stream), then
# their summary is filled in (tools/pgo_summary.py: the stream API writes it as zero).
# Step 3: tools\build_switch.bat <nvk_sdk> pgo-use.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\pgo_merge.ps1 <folder with the .stream files> [-Out <profile folder>]
#
# The profile folder is pgo\ at the root of the repository (SHH_PGO_DIR): one .gcda per object, named by the object's
# path in the build folder with # for each \ (GCC's names with -fprofile-prefix-path). The streams name the profile
# folder of the PC that built the instrumented NRO: merge on that PC.
#
# merge-stream ADDS to .gcda files already there: a second run with the same streams would count them twice, so it
# stops when the profile folder already has .gcda files (move them aside first).
param([Parameter(Mandatory = $true, Position = 0)][string]$Streams, [string]$Out = '')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Out) { $Out = Join-Path $root 'pgo' }
$tool = 'C:\devkitPro\devkitA64\bin\aarch64-none-elf-gcov-tool.exe'
if (-not (Test-Path $tool)) { throw "gcov-tool not found: $tool" }
$files = Get-ChildItem -Path $Streams -Filter 'shh_pgo_*.stream' -File | Sort-Object Name
if (-not $files) { throw "no shh_pgo_*.stream in $Streams" }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
if (Get-ChildItem -Path $Out -Filter *.gcda -File | Select-Object -First 1) {
    throw "$Out already has .gcda files: move them aside first (merge-stream would add to them)"
}
foreach ($f in $files) {
    "merging $($f.Name) ($([math]::Round($f.Length / 1MB, 1)) MB)"
    & $tool merge-stream $f.FullName
    if ($LASTEXITCODE) { throw "gcov-tool merge-stream failed on $($f.FullName)" }
}
$count = (Get-ChildItem -Path $Out -Filter *.gcda -File).Count
if (-not $count) { throw "no .gcda file in $Out after merging: the streams name another folder (merge on the PC that built the instrumented NRO)" }
# The PATH's python can be MSYS's, which takes no Windows paths: the one build_switch.bat uses.
$py = $env:SHH_PYTHON
if (-not $py) { $py = 'C:\msys64\mingw64\bin\python.exe' }
& $py (Join-Path $root 'tools\pgo_summary.py') $Out $files.Count
if ($LASTEXITCODE) { throw 'pgo_summary.py failed' }
"$($files.Count) session(s) merged into $count file(s) in $Out"
