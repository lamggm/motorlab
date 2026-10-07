#!/bin/sh
set -eu
# Provide a .NET 8 SDK executable; no system installation is performed here.
if [ "$#" -ne 1 ]; then
  echo 'Usage: tools/build_compiler.sh /absolute/path/to/dotnet-sdk/dotnet' >&2
  exit 2
fi
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$1" publish "$task_root/compiler/MotorLab.Compiler.csproj" -c Release -r linux-x64 --self-contained true -o "$task_root/libexec/compiler" --nologo

cp "$task_root/compiler/notices/LICENSE.txt" "$task_root/compiler/notices/THIRD-PARTY-NOTICES.txt" "$task_root/libexec/compiler/"
