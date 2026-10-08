# Backend compartilhado — 0.8.0

A GUI usa o `motorlabd` diretamente pelo socket Unix, sem executar comandos de
terminal `motorlabctl` como subprocessos. A CLI e o MCP usam a mesma API do daemon.

```text
GUI (RemoteSession) ──┐
motorlabctl ──────────┼── motorlabd → Session POSIX → USB/RS232 → Roboteq
MCP stdio ────────────┘
```

Um daemon novo começa desconectado e sem permissões de escrita/movimento.
Abrir uma janela ligada a um daemon existente mostra seu estado e seus rascunhos;
não reconecta a USB, aplica configuração ou retoma um slider. Parâmetros lidos,
edições pendentes, script, permissões, RPM Hall/encoder e jobs são compartilhados.
Os gráficos da janela conservam apenas as amostras observadas por essa janela;
a exportação CLI usa o histórico do daemon. Consultas não são simultâneas.

## Exclusividade e encerramento

Leitura completa, aplicação na RAM e manutenção pela GUI reservam uma operação
no daemon. Jobs da CLI/MCP e outras janelas não podem intercalar comandos nessa
operação. Sliders mantidos reservam seu canal; outras janelas não podem assumir
o mesmo canal. Operações gerais exigem parar os canais. Status, telemetria, logs
e acompanhamento de jobs continuam disponíveis. Emergência permanece prioritária,
exceto durante gravação USB DFU, que não deve ser interrompida.

Fechar uma janela observadora não desconecta a USB nem cancela um job da CLI/MCP.
Ao fechar uma janela com seus próprios sliders ativos, o daemon envia zero em
G/S/GIQ ou MS em posição apenas para os canais dessa janela. Esses comandos são
pedidos de parada; verificar o estado físico continua necessário. Script/ajuste
iniciado pela GUI recebe pausa/cancelamento e EX ao encerrar seu cliente.

Cada janela renova sua presença a cada 100 ms. Se desaparecer sem encerrar, após
2 segundos o daemon solicita EX quando ela possuía movimento ou uma transação
com escrita. Perder uma janela somente observadora ou uma leitura não dispara EX.
Uma resposta serial pendente pode obrigar o transporte a fechar sem confirmação,
conforme a proteção existente contra associação incorreta de ACKs. O watchdog do
driver continua necessário: a presença do cliente não substitui sua proteção.
Perda do daemon, timeout de IPC ou eventos expirados desarmam o cliente e não
repetem uma mutação de resultado incerto nem retomam comandos automaticamente.

O fluxo USB DFU é executado como job do daemon também quando iniciado pela GUI.
Ele mantém a validação da imagem, backup parcial, preparo seguro e comparação
byte a byte. Esta mudança foi testada em simulação, sem atualizar um driver real.

## Endpoints e compatibilidade

- Daemon padrão: `$XDG_RUNTIME_DIR/motorlab-cli/control.sock`.
- Janela/compatibilidade MCP: `$XDG_RUNTIME_DIR/motorlab/control.sock`.
- Sem runtime válido: os mesmos sufixos em `~/.local/state`.
- `motorlab --backend-socket /caminho/privado/control.sock` seleciona outro daemon.
- `--backend gui` no MCP encaminha todas as operações ao daemon; `motorlab_view`
  continua atendido pela janela para visibilidade, página e captura PNG.

Diretórios são privados (0700), endpoints restritos ao usuário, sem TCP. A GUI pode
iniciar um daemon ausente sob o mesmo `startup.lock` do cliente Python. Um daemon
antigo ativo não é substituído automaticamente: atualize-o com a unidade parada
e desconectada. Não há fallback da GUI para abrir a serial diretamente.

## Verificação

`shared_backend_tests` usa um daemon separado e pseudoterminal simulado. Verifica
sincronização nas duas direções, aplicação com releitura, Hall e encoder, canais
mantidos, exclusividade, emergência, fechamento normal, perda de clientes e perda
do daemon sem replay. As suítes anteriores continuam cobrindo os contratos do
protocolo e da GUI. Nenhum desses ensaios conecta o hardware da bancada.
