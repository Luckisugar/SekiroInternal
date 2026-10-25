# Builds SekiroInternal.dll and Inject.exe with VS 2022 BuildTools.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
New-Item -ItemType Directory -Force -Path (Join-Path $root 'build') | Out-Null

$vc = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vc)) { throw "VS 2022 BuildTools vcvars64.bat was not found." }

$dllSrc = @(
    'src\dllmain.cpp',
    'src\overlay.cpp',
    'src\life.cpp',
    'src\player.cpp',
    'src\log.cpp',
    'src\i18n.cpp',
    'third_party\hde\hde64.c',
    'third_party\imgui\imgui.cpp',
    'third_party\imgui\imgui_draw.cpp',
    'third_party\imgui\imgui_tables.cpp',
    'third_party\imgui\imgui_widgets.cpp',
    'third_party\imgui\backends\imgui_impl_dx11.cpp',
    'third_party\imgui\backends\imgui_impl_win32.cpp'
) -join ' '

$inc = '/I src /I third_party\imgui /I third_party\imgui\backends /I third_party\hde'
$defs = '/DNDEBUG /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS'
cmd.exe /c "call `"$vc`" && cl /nologo /std:c++17 /O2 /MD /LD /EHsc /utf-8 /W3 $defs $inc $dllSrc /Fo:build\ /Fe:build\SekiroInternal.dll /link user32.lib gdi32.lib imm32.lib d3d11.lib dxgi.lib"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Force -Path (Join-Path $root 'build\exe') | Out-Null
cmd.exe /c "call `"$vc`" && cl /nologo /std:c++17 /O2 /MD /EHsc /utf-8 /W3 $defs /I src src\inject.cpp src\log.cpp /Fo:build\exe\ /Fe:build\Inject.exe /link user32.lib"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item 'build\SekiroInternal.dll' 'SekiroInternal.dll' -Force
Copy-Item 'build\Inject.exe' 'Inject.exe' -Force
Write-Host "Built SekiroInternal.dll and Inject.exe"
