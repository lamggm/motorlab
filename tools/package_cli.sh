#!/bin/sh
# Package an already built headless backend; never touches a running application.
set -eu
if [ "$#" -ne 2 ]; then
  printf '%s\n' 'Uso: package_cli.sh /caminho/motorlabd /destino/pacote-cli' >&2
  exit 2
fi
motorlab_source=$(CDPATH= cd -P "$(dirname "$0")/.." && pwd)
motorlab_binary=$(readlink -f "$1")
motorlab_target=$(realpath -m "$2")
test -x "$motorlab_binary"
if [ -e "$motorlab_target" ] || [ -L "$motorlab_target" ]; then
  printf '%s\n' 'O destino já existe; escolha uma pasta nova.' >&2
  exit 2
fi
test -x "$motorlab_source/libexec/compiler/motorlab-compiler"
mkdir -p "$motorlab_target/bin" "$motorlab_target/mcp" "$motorlab_target/data" "$motorlab_target/tools" "$motorlab_target/docs" "$motorlab_target/libexec"
cp "$motorlab_binary" "$motorlab_target/bin/motorlabd"
cp "$motorlab_source/motorlabctl" "$motorlab_target/"
cp "$motorlab_source/mcp/server.py" "$motorlab_source/mcp/cli.py" "$motorlab_target/mcp/"
cp "$motorlab_source/data/mcp-tools.json" "$motorlab_target/data/"
cp "$motorlab_source/tools/continuous_supervisor.py" "$motorlab_target/tools/"
printf '%s\n' '# MotorLab CLI 0.7.0' '' 'Backend Linux sem interface gráfica, compatível com as 25 ferramentas MCP.' '' 'Execute `./motorlabctl status` para iniciar desconectado.' '' '[Comandos e compilação](docs/CLI.md) · [MCP](docs/MCP.md)' > "$motorlab_target/README.md"
cp "$motorlab_source/docs/CLI.md" "$motorlab_source/docs/MCP.md" "$motorlab_source/docs/CONTROLE-POSICAO.md" "$motorlab_target/docs/"
cp "$motorlab_source/LICENSE" "$motorlab_source/THIRD_PARTY.md" "$motorlab_source/VALIDACAO.md" "$motorlab_target/"
cp -a "$motorlab_source/libexec/compiler" "$motorlab_target/libexec/"
(
  cd "$motorlab_target"
  find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
printf '%s\n' "$motorlab_target"
