#!/usr/bin/env bash
#
# Generate compile_commands.json so clangd resolves the arm-none-eabi / libgba
# headers and stops flagging false errors. This is an IDE convenience only — the
# real build uses arm-none-eabi-gcc via the Makefile and ignores this file.
#
# The output is gitignored (it contains absolute, machine-specific paths). Re-run
# this script after cloning and whenever you add or remove source files.
#
# Honours $DEVKITPRO (defaults to the standard /opt/devkitpro install).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DKP="${DEVKITPRO:-/opt/devkitpro}"

flags=(
	--target=arm-none-eabi
	-mthumb -mcpu=arm7tdmi
	-isystem "$DKP/devkitARM/arm-none-eabi/include"
	-isystem "$DKP/libgba/include"
	-I"$ROOT/source"
	-I"$ROOT/source/ff15"
	-Wno-unknown-attributes
)

# Build the shared JSON "arguments" prefix: "clang", "<flag>", ...
base='"clang"'
for a in "${flags[@]}"; do base+=", \"$a\""; done

out="$ROOT/compile_commands.json"
{
	echo "["
	sep=""
	for f in "$ROOT"/source/*.c "$ROOT"/source/ff15/*.c; do
		[ -e "$f" ] || continue
		printf '%s  {"directory": "%s", "file": "%s", "arguments": [%s, "%s"]}\n' \
			"$sep" "$ROOT" "$f" "$base" "$f"
		sep=","
	done
	echo "]"
} >"$out"

echo "wrote $out ($(grep -c '"file"' "$out") entries)"
