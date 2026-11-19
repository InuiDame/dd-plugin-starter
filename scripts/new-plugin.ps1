#Requires -Version 5
# 从 PluginTemplate 复制出一个新插件目录。
#   .\scripts\new-plugin.ps1 -Name MyPlugin
#   .\scripts\new-plugin.ps1 -Name MyPlugin -Mod MyMod -Version 1.0.0

param (
	[Parameter(Mandatory)][ValidatePattern('^[A-Za-z][A-Za-z0-9_]*$')][string]$Name,
	[string]$Mod,
	[ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$Template = Join-Path $Root 'PluginTemplate'
$Target = Join-Path $Root $Name

if (!(Test-Path (Join-Path $Template 'xmake.lua'))) {
	throw "PluginTemplate not found: $Template"
}

if (Test-Path $Target) {
	throw "Folder with same name exists: $Target"
}

New-Item -ItemType Directory $Target | Out-Null
Copy-Item (Join-Path $Template 'src') (Join-Path $Target 'src') -Recurse

$Lua = [IO.File]::ReadAllText((Join-Path $Template 'xmake.lua'))
$Lua = $Lua -replace 'target\("PluginTemplate"\)', "target(`"$Name`")"
$Lua = $Lua -replace 'set_version\("[^"]*"\)', "set_version(`"$Version`")"
if ($Mod) {
	$Lua = $Lua -replace '(set_version\("[^"]*"\)\r?\n)', "`$1`tset_values(`"dd.mod`", `"$Mod`")`n"
}
[IO.File]::WriteAllText((Join-Path $Target 'xmake.lua'), $Lua)

Write-Host "Generated new plugin <$Name> at $Target" -ForegroundColor Green
Write-Host "Build with: xmake build $Name" -ForegroundColor Green
