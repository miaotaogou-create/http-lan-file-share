# 打单文件绿色版：windeployqt + Enigma Virtual Box
# 依赖：本机已装 Qt、VS、Enigma Virtual Box、Node(generate-evb)
param(
  [string]$QtBin = "C:\Qt6_10\6.10.1\msvc2022_64\bin",
  [string]$Enigma = "C:\Program Files (x86)\Enigma Virtual Box\enigmavbconsole.exe",
  [string]$OutName = "HttpLanFileShare-Portable.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
  $root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
$stage = Join-Path $root "pack\stage"
$outDir = Join-Path $root "pack\dist"
$evb = Join-Path $root "pack\HttpLanFileShare.evb"
$singleExe = Join-Path $outDir $OutName

Get-Process HttpLanFileShare* -ErrorAction SilentlyContinue | Stop-Process -Force

cmd /c "call `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" >nul && set PATH=$QtBin;%PATH% && cmake --build `"$root\build`" --config Release"
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

Remove-Item $stage, $outDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage, $outDir | Out-Null
Copy-Item (Join-Path $root "build\Release\HttpLanFileShare.exe") $stage
& (Join-Path $QtBin "windeployqt.exe") --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage "HttpLanFileShare.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt 失败" }

$crt = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC" -Recurse -Filter "vcruntime140.dll" -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match '\\x64\\' -and $_.FullName -notmatch 'onecore|debug' } |
  Select-Object -First 1
if ($crt) { Copy-Item (Join-Path $crt.Directory.FullName "*.dll") $stage -Force }

Push-Location $env:TEMP
if (-not (Test-Path ".\node_modules\generate-evb")) { npm install generate-evb@1.0.3 --no-save | Out-Null }
Pop-Location

$genJs = @'
const generateEvb = require(process.env.TEMP + "/node_modules/generate-evb");
const path = require("path");
const stage = process.argv[2], evb = process.argv[3], outputExe = process.argv[4];
generateEvb(evb, path.join(stage, "HttpLanFileShare.exe"), outputExe, stage, {
  filter: (full, name, isDir) => isDir || name.toLowerCase() !== "httplanfileshare.exe",
  evbOptions: {
    deleteExtractedOnExit: true,
    compressFiles: true,
    shareVirtualSystem: false,
    mapExecutableWithTemporaryFile: true,
    allowRunningOfVirtualExeFiles: true
  }
});
'@
$genPath = Join-Path $env:TEMP "make-evb-http.js"
Set-Content $genPath $genJs -Encoding UTF8
node $genPath $stage $evb $singleExe
if ($LASTEXITCODE -ne 0) { throw "生成 evb 失败" }

& $Enigma $evb
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $singleExe)) { throw "Enigma 打包失败" }
"{0:N1} MB -> {1}" -f ((Get-Item $singleExe).Length / 1MB), $singleExe
