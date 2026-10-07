#!/bin/sh
set -eu
motorlab_dir=$(CDPATH= cd -P "$(dirname "$0")" && pwd)
# Ordinary opening reuses the MCP-controlled GUI; explicit CLI flags retain
# the standalone executable behavior (demo/screenshots/isolated sessions).
if [ "$#" -eq 0 ]; then
  exec python3 "$motorlab_dir/mcp/server.py" --app
fi
exec "$motorlab_dir/bin/motorlab" "$@"
