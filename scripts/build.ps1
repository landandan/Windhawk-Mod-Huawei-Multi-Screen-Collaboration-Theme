param(
    [string]$WindhawkRoot = 'C:\Program Files\Windhawk'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot 'build'
$compiler = Join-Path $WindhawkRoot 'Compiler\bin\clang++.exe'
$engine = Get-ChildItem -LiteralPath (Join-Path $WindhawkRoot 'Engine') -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!(Test-Path -LiteralPath $compiler) -or !$engine) {
    throw '未找到 Windhawk 编译器或 Engine，请通过 -WindhawkRoot 指定安装目录。'
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$source = Join-Path $projectRoot 'hwmirror-theme.wh.cpp'
$versionMatch = [regex]::Match((Get-Content -Raw $source), '(?m)^// @version\s+(\S+)')
if (!$versionMatch.Success) { throw '源码缺少 @version。' }
$defines = Join-Path $buildDir 'build-defines.h'
@"
#define WH_MOD_ID L"local@hwmirror-theme"
#define WH_MOD_VERSION L"$($versionMatch.Groups[1].Value)"
"@ | Set-Content -Encoding utf8 $defines
$common = @('--target=x86_64-w64-windows-gnu', '-std=c++23', '-DUNICODE', '-D_UNICODE', '-DWH_MOD')
$libraries = @('-ladvapi32', '-lgdi32', '-lgdiplus', '-luser32', '-lcomctl32', '-lmsimg32', '-ldwmapi')
$object = Join-Path $buildDir 'hwmirror-theme.o'
& $compiler @common -Wall -Wextra -Werror -include $defines -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw '模组编译失败。' }
& $compiler --target=x86_64-w64-windows-gnu -shared $object (Join-Path $engine.FullName '64\windhawk.lib') -o (Join-Path $buildDir 'hwmirror-theme.dll') @libraries
if ($LASTEXITCODE -ne 0) { throw '模组链接失败。' }
$testExe = Join-Path $buildDir 'check-hwmirror-theme.exe'
& $compiler @common -DWH_EDITING -static (Join-Path $projectRoot 'tests\check-hwmirror-theme.cpp') -o $testExe @libraries
if ($LASTEXITCODE -ne 0) { throw '回归测试编译失败。' }
& $testExe
if ($LASTEXITCODE -ne 0) { throw '回归测试失败。' }
