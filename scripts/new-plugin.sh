#!/usr/bin/env bash
# Git Bash 版新插件脚本，与 new-plugin.ps1 等价。
#   ./scripts/new-plugin.sh MyPlugin [ModName]
set -euo pipefail

name="${1:-}"
mod="${2:-}"
version="${3:-1.0.0}"

if [[ -z "$name" ]]; then
	echo "usage: $0 <PluginName> [ModName] [Version]" >&2
	exit 2
fi
if [[ ! "$name" =~ ^[A-Za-z][A-Za-z0-9_]*$ ]]; then
	echo "invalid plugin name: $name (must match ^[A-Za-z][A-Za-z0-9_]*$)" >&2
	exit 2
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
template="$root/PluginTemplate"
target="$root/$name"

[[ -f "$template/xmake.lua" ]] || { echo "PluginTemplate not found: $template" >&2; exit 1; }
[[ -e "$target" ]] && { echo "folder already exists: $target" >&2; exit 1; }

mkdir -p "$target"
cp -r "$template/src" "$target/src"

sed -e "s/target(\"PluginTemplate\")/target(\"$name\")/" \
	-e "s/set_version(\"[^\"]*\")/set_version(\"$version\")/" \
	"$template/xmake.lua" > "$target/xmake.lua"

if [[ -n "$mod" ]]; then
	# 在 set_version 之后插入 dd.mod
	sed -i "/set_version(\"$version\")/a\\\tset_values(\"dd.mod\", \"$mod\")" "$target/xmake.lua"
fi

echo "generated $target"
echo "build with: xmake build $name"
