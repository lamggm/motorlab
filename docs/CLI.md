# MotorLab CLI 0.8.0

`motorlabd` é um backend nativo sem janela, Qt GUI/Widgets, X11, Wayland ou
plugin offscreen. Usa Qt Core/Network. `motorlabctl` é o cliente Python 3 de
terminal: retorna JSON e usa exatamente as 25 ferramentas do MCP. GUI, CLI e MCP são clientes do mesmo daemon: conexão, parâmetros pendentes,
permissões, telemetria e jobs são compartilhados em tempo real.

## Começar

Na pasta do pacote, sem precisar de servidor gráfico:

```sh
./motorlabctl status
./motorlabctl ports
```

O primeiro comando inicia o daemon se necessário. Ele começa desconectado,
sem abrir portas nem escrever no driver. Clientes seguintes reutilizam o PID.
Para experimentar com dados simulados:

```sh
./motorlabctl demo
./motorlabctl --wait query '?FID'
./motorlabctl --wait read '~MMOD 1' '~EPPR 1' '~MXRPM 1'
./motorlabctl call telemetry --arguments '{"limit":10}'
./motorlabctl disconnect
```

Para uma unidade real, conecte explicitamente a porta correta. A GUI do MotorLab
pode estar aberta; outros programas que abrem a serial diretamente devem estar
desconectados dela:

```sh
./motorlabctl connect --port /dev/serial/by-id/PORTA-DA-UNIDADE --baud 115200
./motorlabctl status
./motorlabctl --wait query '?S'
./motorlabctl --wait query '?BS 1'
```

`?S` fornece RPM do encoder; `?BS 1` fornece RPM Hall/interno do Motor 1.
Telemetria normal contém ambos. A captura serial legada de 12 campos não
inclui Hall: esse valor aparece como `null`, nunca como zero inventado.

## Todas as operações pelo terminal

`call` aceita o nome com ou sem prefixo `motorlab_` e um objeto JSON conforme
`data/mcp-tools.json`. Exemplos locais, sem transmissão de configuração:

```sh
./motorlabctl call catalog --arguments '{"search":"MAC","limit":10}'
./motorlabctl call stage --arguments '{"changes":[{"query":"~MAC 1","raw":20000}]}'
./motorlabctl call parameters --arguments '{"search":"MAC"}'
./motorlabctl call profile --arguments '{"action":"export_json","path":"/tmp/perfil-motorlab.json"}'
```

Rascunhos usam valores brutos do protocolo; consulte `scale` e `unit` antes
de editar. Importar JSON/CPR também apenas prepara rascunhos. `apply` grava
na RAM; `maintenance` com `command="%EESAV"` persiste separadamente, exigindo
as confirmações e identidade definidas no esquema. Não há aplicação ou
gravação automática ao abrir o backend.

Jobs retornam `job_id`. `--wait` acompanha até concluir, devolvendo código
de saída 0 para sucesso e 1 para falha. Sem `--wait`, consulte `job ID`.
`--timeout` limita a espera do cliente; não cancela nem repete operações.
O resultado do job deve ser verificado antes de repetir uma gravação.

Velocidade e posição continuam disponíveis por `continuous`, com lease e
mesmas guardas. `tools/continuous_supervisor.py` aceita `open_loop`,
`encoder_speed` e `encoder_position`; requer conexão prévia, confirmação de
movimento e identidade. `--help` lista os argumentos. Ele supervisiona ciclos
e renova a lease até a duração solicitada ou Ctrl+C. Um `call continuous`
isolado não mantém movimento: um cliente deve renovar enquanto supervisiona.

`stop` corresponde a **emergência !EX**, removendo torque. Não confundir com
o setpoint zero de velocidade nem com retenção de posição. CLI/MCP mantêm
esse contrato anterior. Consulte [MCP.md](MCP.md) para detalhes e limites.

## MCP

O adaptador usa stdio JSON-RPC, sem dependências Python adicionais:

```sh
python3 mcp/server.py --backend cli
```

O registro já apontando para `mcp/server.py` passa a usar CLI quando o
processo MCP for iniciado novamente; uma instância antiga mantém as opções
com que foi iniciada. Não é necessário mudar o esquema de ferramentas.

`view` permanece no esquema para compatibilidade. Modelo offline e página
são aceitos como metadados; pedidos de mostrar janela ou capturar PNG são
recusados no backend CLI. Para o aplicativo visual, use `--backend gui`.

## Daemon e arquivos

```sh
./bin/motorlabd --help
./bin/motorlabd --control-socket /DIRETORIO-PRIVADO/control.sock
./motorlabctl --socket /DIRETORIO-PRIVADO/control.sock --no-launch status
```

Diretório do socket: propriedade do usuário e modo 0700. O padrão CLI é
`$XDG_RUNTIME_DIR/motorlab-cli/control.sock`; fallback em `~/.local/state`.
A GUI usa esse daemon e pode observar uma conexão iniciada pela CLI. Seu socket
`motorlab/control.sock` é somente uma ponte de compatibilidade para comandos
e operações de janela; não abre outra serial. Não existe porta TCP aberta.
Use `motorlab --backend-socket /DIRETORIO-PRIVADO/control.sock` para selecionar
um daemon isolado. [Arquitetura e ciclo de vida](BACKEND-COMPARTILHADO.md).

O daemon permanece ativo entre comandos; `disconnect` fecha a unidade.
SIGINT/SIGTERM encerra o processo e fecha a Session. Uma operação DFU em
andamento adia esse encerramento até terminar. Movimento deve ser encerrado
pelo comando apropriado e confirmado antes de desligar o daemon; encerramento
do processo não é uma confirmação física de parada. Watchdog permanece ativo.

## Compilar e empacotar só CLI

```sh
cmake -S . -B build-cli -DCMAKE_BUILD_TYPE=Release -DMOTORLAB_BUILD_GUI=OFF
cmake --build build-cli -j4
ctest --test-dir build-cli --output-on-failure
cmake --install build-cli --prefix .
./tools/package_cli.sh build-cli/motorlabd /DESTINO/motorlab-cli-0.8.0
```

Para produção sem Qt Test, adicione `-DBUILD_TESTING=OFF`. Qt Widgets não é
requerido nessa configuração. C++17, CMake ≥3.20, Qt Core/Network ≥6.4 e
Python 3 são necessários. O binário entregue nesta estação é Linux x86_64,
Qt 6.11.2 e glibc 2.42, com dependências dinâmicas; outras distribuições
podem precisar recompilar. O pacote inclui o compilador MicroBasic e runtime
.NET autocontido quando disponível em `libexec/compiler`; para criá-lo no
checkout use `tools/build_compiler.sh`. `dfu-util` é opcional para USB DFU.

## Cobertura e validação

A versão é totalmente CLI para as funções já implementadas no MotorLab:
parâmetros, RAM/EEPROM, perfis, consultas/telemetria, comandos supervisionados,
scripts/compilação/simulação/upload, leitura CANopen e fluxo USB DFU.
Ela não completa funções ainda ausentes do projeto: firmware serial/CAN,
ajuste automático e linearidade permanecem pendentes. USB DFU não foi
validado fisicamente nesta entrega. Nenhum ensaio deste desenvolvimento
acessou ou modificou o motor da bancada.

Suítes: `shared_backend_tests` verifica GUI/CLI, edição, releitura, sliders e perda de clientes no mesmo daemon; `headless_tests` valida guardas e jobs por pseudoterminais;
`tools/test_mcp.py` verifica as 25 ferramentas e clientes concorrentes;
`tools/test_cli.py` verifica terminal sem display, perfis, telemetria e saída
de erro. Consulte `VALIDACAO.md` para os resultados desta versão.
