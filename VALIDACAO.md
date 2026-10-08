# MotorLab Linux — validação

## GUI, CLI e MCP no mesmo daemon — 0.8.0, 08/10/2026

- A GUI de produção usa `RemoteSession`, cliente do socket de `motorlabd`; não abre uma segunda serial. O endpoint MCP da janela encaminha comandos ao daemon e atende visibilidade/captura localmente. Duas janelas e clientes CLI/MCP observam o mesmo PID, conexão, permissões, rascunhos, script e telemetria.
- Configuração pela GUI tem exclusividade desde o diálogo de revisão até o fim da escrita/releitura. Recibos de enfileiramento evitam tratar um evento anterior como ACK de uma nova solicitação. Emergência revoga uma operação gráfica em andamento; comandos de resultado incerto não são repetidos.
- CTest final: **6/6 suítes aprovadas**, incluindo protocolo, features, headless, GUI, control e a nova `shared_backend_tests` (8 cenários, mais inicialização/encerramento). A nova suíte testa sincronização nos dois sentidos, RAM/releitura, Hall/encoder, proteção da revisão, canais mantidos, fechamento normal com zero, exclusividade, perda de clientes e daemon sem replay, e descarte de respostas antigas.
- Integrações Python: **8/8 MCP**, **3/3 CLI** e **2/2 GUI/daemon** aprovadas. Os últimos dois verificam o gateway, captura PNG, reinício da janela preservando o daemon e autostart concorrente de duas janelas com um único daemon desconectado.
- `ldd` continua sem QtGui/QtWidgets no daemon; o teste CLI passou com um plugin gráfico inexistente. As 25 ferramentas existentes foram preservadas.
- Evidências locais: `../../work/build-shared-backend/Testing/Temporary/LastTest.log`, `../../work/tests-shared-final-ctest.log` e `../../work/tests-shared-final-gateway.log`. Ensaios exclusivamente em demonstração/pseudoterminais. Driver real, EEPROM e firmware não foram alterados nesta entrega; DFU pela GUI agora delega ao job existente do daemon, sem validação física nova.
- [Arquitetura e ciclo de vida dos clientes](docs/BACKEND-COMPARTILHADO.md). Firmware serial/CAN, ajuste automático e linearidade continuam fora da cobertura implementada.

## CLI 0.7.0 — 08/10/2026

- `motorlabd` usa QCoreApplication e Qt Core/Network. Configuração `MOTORLAB_BUILD_GUI=OFF` compila sem exigir Qt Widgets. `ldd` confirmou ausência de QtGui, QtWidgets, X11, xcb e Wayland no executável; binário x86_64 nesta estação, Qt 6.11.2 e glibc 2.42.
- `ControlHost` separa a sessão e os dados dos widgets. O backend CLI e o adaptador GUI usam o mesmo `ControlServer`, protocolo, filas, catálogo e supervisão; a GUI instalada foi preservada.
- Protocolo e features passaram; `headless_tests` usa exclusivamente demo/pseudoterminais para RAM, EEPROM, contadores, pausa/EX/FF/FS, modos encoder_speed/encoder_position, leases, perdas de comunicação, corrente, falhas e ausência de progresso. Captura legada identifica DI/DO corretamente e mantém Hall ausente como null.
- Oito integrações MCP passaram sem DISPLAY/WAYLAND: descoberta das 25 ferramentas, lançamento concorrente com um PID, desconexão inicial, catálogo/leitura dos 638 campos, compartilhamento de rascunhos entre clientes, compilador/perfis, lease e socket privado. Cliente oficial `mcp==1.26.0` confirmou initialize/list/status/demo/compiler/job/error.
- Três integrações do terminal passaram com `QT_QPA_PLATFORM` apontando para um plugin inexistente: daemon sem interface gráfica, RAM demo e releitura, perfis JSON/CPR, importação atômica e modelo incompatível, CSV/logs, simulação MicroBasic, rejeição de janela/PNG e códigos de saída em falhas. SIGTERM encerrou os daemons e removeu seus sockets.
- A regressão do adaptador GUI teve 81 casos aprovados; o único erro inicial foi ausência de `MOTORLAB_COMPILER` ao invocar a suíte diretamente fora do pacote. Reexecutar o cenário com o helper explícito passou (3/3 incluindo init/cleanup). CTest já configura esse caminho.
- Evidências locais em `../../work/tests-cli-release.log`, `tests-mcp-cli.log`, `tests-terminal-cli-final.log`, `tests-sdk-cli.log`, `tests-gui-control-host.log` e `tests-gui-compiler-host.log`. Esses logs são locais, não acompanham o Git.
- `motorlabctl` e o MCP iniciam CLI por padrão, em socket próprio `motorlab-cli/control.sock`; GUI continua disponível com `--backend gui`. Porta física permanece exclusiva. Nenhum teste desta entrega conectou, movimentou, configurou ou gravou EEPROM/firmware na bancada.
- CLI cobre as funções implementadas; não completa firmware serial/CAN, ajuste automático ou linearidade. Fluxo USB DFU assíncrono tem validação de imagem, identidade USB e comparação por leitura, sem ensaio físico nesta entrega.


# Posição salva na EEPROM e comando manual — 07/10/2026

A pedido do operador, Motor1 configurado em MMOD3, MVEL30 RPM, MXRPM100 RPM e KPG3=0,2; contador A/B zerado em malha aberta e com motor parado. `%EESAV` confirmado por ACK;636 parâmetros relidos, somente as quatro diferenças previstas. Sem ciclo de energia; zero mecânico não persistente pela interface incremental. Motor final parado, FF16, controle manual desligado.

GUI P renova destino absoluto confirmado com G0/S/P, aceita zero e separa rascunho do alvo ativo; faixa±4000contagens. MS preserva autorização e canal2, sem P0/EX. UI5/5 e protocolo19/19 aprovados; binário recarregado,636 valores preservados. Movimento manual físico fica para o teste do usuário. [Detalhes e evidências](docs/CONTROLE-POSICAO.md).

# Controle de posição — 07/10/2026

Motor 1 ensaiado em Count Position com referência temporária, MVEL30 RPM e ganho P de posição0,2. Destino90° resultou89,91°; retorno0° resultou0,36°, enquanto energizado. Operador confirmou avanço e retenção; FF/FM0 durante as capturas. Após EX, retorno acomodou em1,08°; sem torque não há retenção garantida. Aproximação fina no retorno ainda lenta. Todos os636 parâmetros anteriores restaurados/relidos sem diferenças, modo velocidade e máximo3000 RPM, desaceleração2000 RPM/s. Referência incremental anterior restaurada com o deslocamento físico residual. Nenhuma escrita EEPROM nesta rodada.

MCP ganhou posição absoluta supervisionada e manutenção de contador com ambos os canais em malha aberta. Protocolo18/18 e posição11/11 aprovados. Consolidado43 aprovados/1 falha intermitente em regressão de velocidade: guarda de atraso da comunicação acionou EX confirmado no simulador; não se afirma aprovação total. [Relatório, configuração experimental e evidências](docs/CONTROLE-POSICAO.md).

# Sintonia de baixa velocidade — 07/10/2026

Motor 1 ajustado e ensaiado a 20, 100, 500, 1500 e 3000 RPM. Operador aceitou o giro lento observado na bancada; ainda há oscilação residual. Desaceleração normal ajustada para 2000 RPM/s a pedido do usuário e confirmada por leitura. Configuração apenas na RAM, EEPROM não gravada; motor deixado parado. [Relatório completo e parâmetros](docs/SINTONIA-20RPM.md).

# Sintonia de aceleração e captura rápida — 07/10/2026

Motor 1 SBLG2360T v3.0f, Kalatec KTC-86BLS98-J/X046, AS5047P 1000 PPR, bancada sem carga e sentido positivo. Leitura inicial encontrou MMOD0; selecionado MMOD1. Ganho integral FOC de fluxo/torque aumentado gradualmente de 6,2832 para 25,1328; Kp FOC0,1 e PI velocidade0,03/0,01 preservados. ALIM1=11,5 A durante os ensaios e no perfil final; aceleração3000 RPM/s, desaceleração200 RPM/s, MXRPM3000. EEPROM não gravada nesta rodada.

Com a mesma rampa de3000 RPM/s, os ganhos iniciais provocaram LoopErr com erro amostrado1192 RPM; no ajuste final, duas partidas a3000 RPM completadas sem LoopErr. Repetição15s: 95% do alvo em1,288s, primeira amostra3000 RPM em1,446s, pico3056 RPM, erro máximo448 RPM, média pelo contador nos últimos2s3000,86 RPM. Proteção500 RPM/500ms mantida. Ensaio final100 RPM: média pelo contador100,15 RPM; leituras instantâneas76–122 RPM. Operador confirmou giro suave durante a rodada. PWM próximo de100% e AmpLim intermitente no topo; não validado sob carga, em reversão ou termicamente. Indutância desconhecida, RS/LD/LQ não modificados.

capture_fast opcional mantém até2000 ciclos completos em job.samples, inclui referência/erro/PWM/Id/Iq e interrompe por LoopErr ou corrente FOC excessiva. Taxa real final~38–40 Hz, consultas sequenciais não simultâneas. GUI destaca erros FM mesmo com FF0; AmpLim é estado de limitação e FETs Off isolado não vira erro de motor. Parar normal permanece zero, supervisor permanece EX. Testes de pseudoterminal:33 aprovados na supervisão existente e captura inicial;5 aprovados na captura final (LoopErr e excesso de corrente nos dois eixos FOC);5 aprovados na interface e regressão dos sliders. Logs TESTES-sintonia-*.log.

Recarregada GUI com motor parado;637 valores brutos preservados na recarga final. Comparação começo/fim confirmou somente ALIM1, MMOD1, KIF1 e KIF3 diferentes; Motor2 e demais proteções preservados. Estado final FF16, FM1=128, encoder0:0 RPM, Hall1=0 RPM; permissões de movimento/escrita desligadas. Relatório, perfis, capturas JSON/CSV e gráfico antes/depois em ../sintonia-bancada-2026-10-07/resultado.txt.

# Gravação EEPROM sem script carregado — 07/10/2026

Corrigidos GUI e MCP para %EESAV quando !R 0 é rejeitado por ausência de script. Antes da gravação, exige ACK de !EX, ?FF recente com EStop e ?FS recente sem script/setup; estados inválidos ou rejeitados impedem %EESAV. Outras manutenções conservam a pausa estrita. ACK da EEPROM é necessário para sucesso. Os testes abrangem pausa aceita/rejeitada, falhas de parada, script/setup ativos, dados inválidos e rejeição da própria gravação. GUI + regressão RAM: 21 testes aprovados (10,269 s); MCP + regressão RAM/autorização: 18 aprovados (9,295 s). Logs TESTES-eeprom-ui-tests.log e TESTES-eeprom-control-tests.log.

Gravação real em SBLG2360T v3.0f: %EESAV recebeu ACK + em 07/10/2026 16:17:22 (America/Sao_Paulo), após !EX/FF16/FS8. Recarga da GUI preservou 637 valores lidos, incluindo EPPR1000, PI velocidade0,03/0,01 e PI fluxo/torque0,1/6,2832. Nova leitura após salvar confirmou os mesmos valores. Motor permaneceu parado; não houve reinício do driver nem teste de retenção após ciclo de energia. Evidências em ../eeprom-confirmada-2026-10-07/.

# Ações normais e avançadas — 07/10/2026

O painel inicia com Ler do controlador, Habilitar edição, Aplicar alterações (RAM), Salvar permanente (EEPROM), Salvar perfil e Abrir perfil. Modo avançado revela as ações especializadas, preservando seus comandos, objetos e verificações existentes. Textos e tooltips distinguem rascunhos, RAM, EEPROM, calibrações na Flash e arquivos locais. EEPROM não aplica automaticamente os rascunhos; CLSAV não inicia calibração. Manual v3.0c pp. 344–346 conferido.

Capturas em demonstração inspecionadas nos modos normal/avançado a 1100 e 1440 px; suíte ui_tests passou em 61,01 s (TESTES-acoes.log). A janela anterior estava desconectada, após erro de permissão USB. A correção de udev específica 20d2:5740 foi instalada no host com autenticação administrativa; esta revisão do painel não instala regras do sistema automaticamente.

Recarga concluída com a sessão anterior desconectada. Nova GUI conectou ao SBLG2360T v3.0f e concluiu consultas: 637/638 respostas aceitas. Escrita e movimento permaneceram desabilitados; nenhum ensaio de movimento ou gravação foi realizado. Captura interface-real.png e resultado.json em ../acoes-normal-avancado-2026-10-07/. Permissão local verificada como usuário lamggm: leitura e escrita em /dev/ttyACM0, root:lamggm 0660. Regra persistente /etc/udev/rules.d/72-motorlab-roboteq.rules; reconexão física/reboot não exercitados.

# Legibilidade da configuração — 07/10/2026

As árvores Entradas/saídas e Saída de potência passam a ocupar abas internas da mesma área ampla, com ações à direita. Colunas Atual/Novo valor/Unidade de 112/250/145 px; nome do parâmetro usa o espaço restante. Fonte dos parâmetros 13 px e dos editores 14 px, maior altura e contraste dos campos, cabeçalhos ajustáveis, popup de opções com largura mínima 430 px e tooltip do texto completo. Busca expande os resultados e troca para a aba correspondente quando a atual não contém resultados. Proteção contra scroll preservada. Suíte ui_tests passou em 60,75 s; log TESTES-layout-configuracao.log.

Capturas de demonstração inspecionadas em 1100×780 e 1440×860, nas duas abas. Na menor janela, viewport da árvore de 799 px e colunas 292/112/250/145; na maior, 1139 px e colunas 632/112/250/145. Valores numéricos e unidades visíveis sem sobreposição. Capturas em ../layout-configuracao-2026-10-07/; dados dessas imagens são simulados.

Recarregado com motores previamente parados. Releitura de 636 parâmetros confirmou valores brutos idênticos; um rascunho preservado sem aplicação. A desconexão normal enviou !EX por estar com controle manual habilitado; a nova sessão iniciou com controle manual desabilitado. Perfil, CSV, console e script preservados na pasta de evidências.

# Proteção contra edição por scroll — 07/10/2026

Campos numéricos, seletores fechados e sliders do aplicativo não modificam valores com a roda do mouse, inclusive após receberem foco. O evento é encaminhado ao viewport da árvore/painel para preservar a rolagem. Clique, teclado, botões de incremento e seleção explícita continuam disponíveis; listas abertas conservam a navegação do popup. A regra inclui configuração, comandos manuais, conexão e inspetor CAN.

Regressão com controlador simulado verificou campos ALIM/MMOD com e sem foco, ausência de rascunhos por scroll, avanço da barra de rolagem, edição explícita por teclado e slider de motor sem comando involuntário. Nenhum comando de configuração/manutenção/movimento foi emitido pelo teste. Log TESTES-scroll-foco.log. Suíte completa ui_tests aprovada em 54,14 s, log TESTES-scroll-interface.log.

Recarga com os dois motores já parados, sem comando mantido não zero no log recente. O fechamento normal da sessão com controle habilitado envia !EX; aplicativo reaberto sem habilitação manual. Releitura confirmou 636 parâmetros idênticos, três rascunhos preservados sem aplicação. Perfil, CSV, console e script anteriores salvos em ../scroll-2026-10-07/.

# Unidades de configuração — 07/10/2026

Revisadas 125 definições a partir do manual de controladores v3.0c e das árvores RoboRun+ 3.3/v3.0. O catálogo não usa mais o rótulo genérico “bruto”: grandezas recebem ms, Hz, mV, Ω, K, %, pulsos/volta, contagens etc.; seletores, máscaras e mapas mantêm sua natureza codificada, explicada nos tooltips. CGT tem unidade por índice. Captura acompanha PMOD; erro de malha acompanha MMOD conhecido. Rascunho que muda o modo evita rotular valores de dois modos diferentes com uma única unidade. Ângulos preservam passos de 1/512 de volta. TID passa a ser mostrado em A, escala ×10 conforme p. 449, sem mudar o inteiro recebido.

Comparação automática do catálogo confirmou preservação de limites, índices, permissões, códigos e demais metadados de protocolo; apenas unidade, notas, unidades por índice e escala de exibição TID mudaram. Teste em pseudoterminal verifica escalas, unidades dependentes de modo e ausência de comandos de configuração, movimento ou manutenção. feature_tests e ui_tests passaram: 2/2 suítes, 56,24 s. Logs TESTES-unidades.log e TESTES-unidades-foco.log. Evidências adicionais em ../unidades-2026-10-07/.

Aplicativo recarregado e reconectado ao SBLG2360T v3.0f. Releitura de 636 parâmetros confirmou valores brutos idênticos aos anteriores; quatro rascunhos foram preservados localmente, sem aplicação. Perfil, console, telemetria e script anteriores foram exportados. A edição na RAM voltou ao estado desmarcado da inicialização. Inspeção visual confirmou a coluna de unidades; nenhum comando de configuração ou movimento foi solicitado nesta revisão.

# Aplicação de STO na RAM — 07/10/2026

Manual de controladores v3.0c, páginas 362–363: configuração global STO, CANopen 0x30CF, escrita serial `^STO 1` para habilitar e consulta `~STO`. Não há índice de motor nesse comando. Valores 0=desabilitado e 1=habilitado.

O caminho gráfico de aplicação RAM abortava antes da escrita quando o firmware rejeitava `!R 0` por ausência de script. Agora segue a mesma verificação já usada no MCP: tentativa de pausa, ACK de `!EX`, confirmação do bit EStop em `?FF` e consulta nova `?FS` sem script/setup ativo. Só então envia a configuração e exige releitura correspondente. Rejeição de EX, ausência de confirmação de parada, script/setup ativo e resposta inválida/rejeitada de FS impedem a escrita. Outras manutenções mantêm o fluxo anterior.

Testes com controlador simulado confirmam `^STO 1` seguido de `~STO`, sem índice extra e sem EEPROM ou liberação de movimento, tanto com pausa aceita como com pausa rejeitada. As falhas acima foram exercitadas; log TESTES-sto-registrador.log. No hardware foi feita apenas consulta: STO=0 em 07/10/2026 às 16:44:40 UTC. Corrigir o caminho de aplicação não habilita STO automaticamente nem valida jumper/entradas/circuito físico.

# Correção dos seletores de configuração — 07/10/2026

STO e outros campos de lista permaneciam bloqueados depois de marcar Habilitar edição na RAM. O editor numérico oculto era filho da lista e seu estado efetivo herdava o bloqueio da própria lista. A interface agora habilita ambos diretamente pelas mesmas condições de permissão, leitura válida e ausência de operação em andamento.

Regressão com controlador simulado reproduziu a falha antes da correção e passou depois: STO lido volta a ser editável, alternar a permissão preserva o rascunho, parâmetros não lidos continuam bloqueados e escolher um valor não transmite comandos de escrita, movimento ou manutenção. Isso valida a interface, não o circuito STO físico. Suíte ui_tests aprovada em 50,36 s. control_tests teve 48 aprovações e uma falha em encoderSpeedSupervision(rated-negative); esse cenário passou na repetição isolada em 2,135 s. Não se declara a execução completa de controle como aprovada. Logs em TESTES-sto-interface.log e ../sto-interface-2026-10-07/. Aplicativo recarregado e reconectado; releitura de 636 parâmetros concluída, STO=0, FF16, Hall/encoder0 RPM. Nenhuma configuração do controlador alterada.

# Faixa nominal de 3000 RPM liberada — 07/10/2026

Substitui os limites temporários de200 RPM/3 A/25% do ensaio abaixo. RAM motor1: MXRPM3000, MXPF/MXPR100%, ALIM11,5 A, aceleração100 RPM/s, desaceleração200 RPM/s. Ganhos de velocidade0,03/0,01 e corrente0,1/6,2832 mantidos ao final. EEPROM não gravada; watchdog1000 ms preservado.

Teste sem carga a3000 RPM: encoder2968–3040, média3001,45 RPM na janela final; FF0. Corrente observada-3,2 a4,4 A; AmpLim intermitente, dissipador37°C. Ensaios anteriores com guarda3 A interromperam por corrente; valores temporários descartados. Estado final verificado conforme pedido: manual habilitado, FF0, S/RPM selecionado, referência0, motor parado. Evidências em ../velocidade-3000rpm-2026-10-07/resultado.txt e configuracao-liberada.json. Não qualifica carga, reversão ou regime térmico prolongado.

Supervisor encoder_speed ampliado até +/-3000 RPM, guarda até3600 RPM/13 A e potência explícita até100%. Padrões conservadores preservados; open_loop mantém G+/-100 e guarda1000 RPM/11 A. Protocol/control43,21 s; oito integrações26,605 s; guardas da faixa nominal17,492 s; SDK oficial25 ferramentas aprovado. Logs TESTES-encoder-3000.log, MCP-TESTES-encoder-3000.log, TESTES-encoder-nominal.log e MCP-SDK-encoder-3000.log.

# Malha de velocidade pelo encoder — 07/10/2026

Motor 1 SBLG2360T v3.0f configurado na RAM: MMOD1=1, EMOD1=18, EPPR1=1000, BLFB1=0; velocidade PI 0,03/0,01; corrente fluxo e torque PI 0,1/6,2832. Limites de bancada 3 A, 200 RPM, 25% potência, aceleração 50 RPM/s e desaceleração 100 RPM/s. RWD1000 e comutação Hall preservados. EEPROM não gravada; L desconhecida, RS/LD/LQ não alterados. Sintonia empírica inicial, não caracterização automática.

Ensaio final de 20 s a 100 RPM, sem carga: contador encoder indicou média 99,98 RPM após transitório; leituras instantâneas 75–124 RPM. Operador confirmou “Giro suave”. Ensaios intermediários inadequados interrompidos pelas guardas, com EX confirmado. Ganhos intermediários descartados. Final parado, FF16, Hall/encoder0 RPM e 0 A. Relatório e evidências: ../malha-velocidade-encoder-2026-10-07/resultado.txt. Sem qualificação sob carga ou em toda a faixa.

MCP contínuo inclui modo encoder_speed com !S, preflight de configuração/limites e guardas de sentido, velocidade e progresso do encoder. Nove cenários novos de pseudoterminal. Build e suítes protocol/control passaram em 37,04 s; oito integrações MCP passaram em 26,631 s. SDK oficial confirmou 25 ferramentas e fluxo initialize/list/status/demo/compiler/job/error. Logs TESTES-encoder-speed.log, MCP-TESTES-encoder-speed.log e MCP-SDK-encoder-speed.log. Binário recarregado e leituras reais verificadas. Parar comum da GUI segue enviando zero, separado de EX do supervisor.

# Slider mantido, Parar normal e RPM Hall/encoder — 07/10/2026

Esta revisão substitui o comportamento anterior de segurar/soltar descrito nas entradas históricas abaixo. O operador pediu comando mantido após soltar e Parar comum sem emergência.

Duas causas de EX identificadas no código: sliderReleased solicitava EX; finish de consulta MCP revogava escrita/movimento mesmo em leitura. Removida a primeira ligação e preservadas permissões existentes ao terminar jobs read. Jobs que disputam a sessão são recusados durante comando mantido pelo slider. Falhas de transporte continuam desarmando.

Slider/Aplicar mantém G/S/GIQ com repetição dentro do watchdog, um comando pendente por canal, sem depender do botão do mouse. Zero/Parar envia zero no modo selecionado (S0 em RPM, G0 em comando bruto, GIQ0 em corrente), sem EX ou revogação; próximo valor pode ser aplicado diretamente. Posição absoluta continua envio único e MS. Emergência, desabilitação, falha e atraso de comunicação interrompem e registram o motivo, sem reinício automático.

Encoder A/B (?S) e Hall/interno (?BS por canal) têm cartões simultâneos, fontes fixas, gráfico selecionável e campos CSV/MCP distintos. Ausência/rejeição não aparece como zero. Captura serial legada de 12 campos não inclui Hall; esse valor fica indisponível durante a captura legada.

Validação: quatro suítes Qt passaram (90 cenários mais oito hooks), 78,38 s. Oito integrações MCP passaram, 26,656 s; SDK oficial confirmou 25 ferramentas. Cobertos latch além de RWD, Parar em G e S sem EX, retomada, dois canais sem starvation, rejeição de job concorrente, consulta sem revogação, falha real com EX e RPM separados/ausentes. Logs TESTES-sliders-latch-rpm.log, MCP-TESTES-sliders-latch-rpm.log e MCP-SDK-sliders-latch-rpm.log.

Binário recarregado PID2533349 com driver parado e sem rascunhos pendentes. Leitura real: SBLG2360T v3.0f, RWD1000, EPPR1=1000, MMOD1=0, BPOL1=4, EMOD1=18, BLFB1=0; nenhum parâmetro/EEPROM alterado nesta revisão. Hall e encoder responderam 0 RPM com FF16. Screenshot inspecionado: ambos os cartões visíveis. Não houve novo ensaio físico de giro nesta revisão; aceitação do novo comportamento pelo operador ainda pendente. A leitura RPM não transforma o modo aberto em malha fechada de velocidade.

# Revisão de acionamento pelos sliders — 07/10/2026

Operador relatou que número mudava sem giro. Implementação anterior apenas editava setpoint ao arrastar; estado observado também FF16,EStop ativo, motion_enabledfalse e valor107 na tela. Não há log suficiente para reconstruir todos os cliques anteriores.
Agora pressionar/arrastar slider em G/S/GIQ envia repetidamente enquanto mantido, apenas com permissão manual e leituraFF0. Soltar solicitaEX. Valor digitado continua local. Enviar uma vez e Segurar para rodar explicitam semântica. Posição absoluta continua envioúnico. Rótulo porcanal exibe ACK/rejeição; ACK não comprova giro.
Parada manual cancela comandos pendentes, drena resposta serial por até250ms, entãoEX; caso de atraso usa caminho prioritário original com possibilidade de desconexão sem confirmação.
Teste PTY da interface: EStop impede envio, confirmaçãoMG permite, slider repeteG100 e acompanha110, soltar duranteconsulta enviaEX e mantém conexão, nenhumG depoisEX. Também valida flags de stream. UI+controle passaram62,65s;8integrações26,648s; ajuste finalstream passou2testes focados3,034s. Não realizado novo acionamento físico nesta revisão.
Binário carregadoPID2502784, controlador identificado SBLG2360T v3.0f, RWD1000,EPPR1=1000,MMOD1=0 confirmados por leitura. Motor deixado parado; sem alteração de parâmetros nesta revisão. Operador deve confirmar comportamento físico do novo gesto.


# EPPR confirmado pelo operador e corrigido — 07/10/2026

AS5047P informado em1000PPR. EPPR1 alterado4000→1000 na RAM, ACK e releitura confirmados. EEPROM não gravada. MMOD1=0,EMOD1=18,BLFB1=0,RWD1000 preservados; motor parado. Job10ac69b2-3e89-48a2-b760-0c5172f77160, evidências em outputs/encoder-controle-manual-2026-10-07.

MCP apply tolera rejeição de R0 apenas se EX/FF confirmarem parada e nova FS comprovar script/setup inativos. Cinco casos de regressão adicionais; suítecontrole22,24s e oito integrações26,674s passaram. Logs TESTES-aplicacao-encoder.log e MCP-TESTES-aplicacao-encoder.log. A rotina de aplicação pela GUI e demais operações mantêm seu fluxo anterior.

# Revisão de controle manual — 07/10/2026

Atalho na aba Monitoramento para habilitar controles sem depender da aba Configuração. Habilitação exige confirmação e watchdog válido; só consulta RWD se ainda desconhecido, sem alterar o valor. Não envia movimento/liberação de EStop. Teste em pseudoterminal cobre cancelamento, habilitação, slider sem transmissão, desabilitação com EX e recusa de watchdog zero. Logs em TESTES-controle-manual.log.

Inspeção real: encoder1 EMOD=18, EPPR=4000; MMOD1=0; BLFB1=0; RWD=1000. Usuário identificou AS5047P A/B no DB25. Resolução ABI real ainda não verificada; nenhum parâmetro foi alterado. Antes desta revisão o controlador já havia parado por revogação de permissão, com EX confirmado. Aplicação recarregada parada; controle manual disponibilizado para operação pelo usuário, sem reiniciar o motor.

# Validação 0.6.0 — 07/10/2026

Modo contínuo start/renew com lease monotônica, guardas e parada. Quatro suítes Qt passaram (79 cenários, mais 8 entradas de setup/cleanup), oito integrações stdio/socket passaram e cliente oficial MCP reconheceu 25 ferramentas. Logs: TESTES-v0.6.log, MCP-TESTES-v0.6.log e MCP-SDK-v0.6.log.

Ensaio real autorizado no SBLG2360T v3.0f: comando G1=100 em malha aberta, watchdog1000ms, supervisão3s e limites10A/600RPM/60°C/2s sem progresso. Sensor interno indicou aproximadamente280RPM e usuário confirmou giro visível. Deixado sob supervisão a pedido do usuário. Esta execução não comprova malha fechada de100RPM nem equivalência completa ao RoboRun+. Evidências externas ao pacote: outputs/teste-continuo-2026-10-07.

Interface contínua: cartões atualizados pelo sensor interno, com gráfico/I/O explicitamente pausados. Parada manual duplicada é idempotente e o job final deixa de indicar running. Revisão final: CTest 61,43 s, oito integrações Python 26,664 s. Ensaio inicial teve EX confirmado antes da recarga; mesmo comando retomado no job 00ebb1a0-20fd-42d6-800b-5cf84bc9b744. Registros são históricos: consultar status para o estado presente.

Correção da finalização execute: EX e verificação FF precedem pausa de script. Resposta negativa de !R0 permanece visível como falha parcial.

## Histórico das validações anteriores

# Validação — MotorLab Linux 0.5.0 (MCP)

07/10/2026. **62 cenários C++/Qt passaram**, mais oito hooks de preparação/finalização: protocolo 16, recursos 21, interface 17, controle MCP 8. CTest: 4/4 suítes, 46,60 s. Oito testes de controle cobrem validação de esquema/rascunhos atômicos, identidade exata e RAM, FF/releitura divergentes, movimento limitado/watchdog, compilador/perfis/upload, retomada da telemetria após jobs, parada durante resposta pendente e firmware desconhecido/injeção.

**6 testes de integração stdio/socket passaram**, incluindo inicialização concorrente sem conexão automática, dois clientes na mesma GUI, 638 leituras, compilador, CPR e PNG. O **cliente oficial MCP Python 1.26.0** negociou protocolo 2025-11-25 e validou descoberta das 24 ferramentas, demo, compilação/simulação, job e erro de consulta indevida.

Executável 0.5.0 iniciado pelo adaptador registrado: control_api_version=1 e connected=false. Janela mostrada e captura nativa inspecionada. Nenhum dispositivo físico, movimento, gravação de Flash/EEPROM ou barramento CAN real foi testado. USB DFU por MCP reutiliza o fluxo existente, mas continua sem validação física. Firmware serial/CAN e ajuste/linearidade completos continuam pendentes.

Logs atuais: TESTES.log e MCP-TESTES.log. Guia: docs/MCP.md. A comparação do compilador e os sanitizers abaixo são evidências da versão 0.4, não uma execução nova de sanitizers do servidor MCP.

---

# Validação — MotorLab Linux 0.4.0

07/10/2026 · Linux x86_64 · GCC 16.2.1 · Qt 6.11.2 · .NET 8.0.31.

**54 cenários C++/Qt passaram**, além de seis hooks de preparação/finalização: protocolo 16, recursos 21 e interface 17. CTest: 100%, 3/3 suítes, 42,68 s. Registro original da versão 0.4; TESTES.log agora contém a execução atual da versão 0.5. Nenhum controlador físico foi aberto, movimentado ou programado.

## Engenharia reversa e compilador

- 14 casos comparados com a DLL MicroBasic.dll original: aritmética, arrays/For, If, GoSub, comandos/configuração, Boolean, erro de sintaxe, CRLF e os seis scripts originais de ajuste do instalador. Bytecode, HEX, tamanho, CRC e diagnósticos coincidiram. A simulação dos casos curtos também foi comparada.
- 18 verificações do helper passaram: os 14 casos acima mais limites de laço infinito, Wait longo, saída excessiva e memória de variáveis. Logs em COMPILADOR.log; detalhes da comparação em docs/compiler-differential.json.
- Simulação com SetCommand usa apenas o console local; teste GUI conectado a pseudoterminal comprovou que não enviou comando de motor. Exportar não executa nem transmite o script.
- O helper é ELF Linux x86_64, com runtime autocontido. Dependências locais resolvidas; nenhum Wine ou componente WinForms carregado no aplicativo.

## Configuração e CPR

- 638 campos numéricos, 155 definições, 153 editáveis. Índices DINL 1–10 / níveis 0–1 e EE 1–32 conferidos; comandos sem índice, fora da faixa e opções ocultas/vazias são recusados.
- Hierarquia original, listas textuais e escalas de motor verificadas. Bitmasks CheckComboBox não foram confundidos com enumerações simples.
- Perfil original SBLG2360T: 613 itens incluindo aliases, 605 parâmetros únicos. Round-trip mantém valor bruto, multiplicador, tipo e caminho; 600 rascunhos compatíveis importados pela GUI, sem TX. TELS e quatro DOA ficam identificados como não importados.
- XML truncado, DTD, duplicatas, aliases conflitantes, valores fora de 32 bits e índices inválidos recusados. Exportação parcial conserva escala ×10 de ALIM e origem DEMO, inclusive após desconectar.
- Exportação RoboCAN confere pedido, ordenação dos bytes/endereço, ACK por bloco de 16 bytes e commit. Não transmite frames e não emite liberação de emergência.

## Regressão da versão anterior

Passaram identidade/firmware, transporte serial em pseudoterminais, exclusividade, fragmentação/eco/timeout, RAM com ACK/releitura nos dois modelos, pausa/EX/FF antes de mutações, rejeição de parada não confirmada, HEX/HLD/ACK sem execução automática, imagens DFU/modelo/hash, SDO, watchdog, captura/CSV, isolamento demo/real, unidades, limites de gráficos e simulador parcial anterior.

O núcleo passou também em Debug com AddressSanitizer e UndefinedBehaviorSanitizer: **37 cenários**, duas suítes, 6,14 s, sem diagnóstico de acesso inválido ou comportamento indefinido. LeakSanitizer desabilitado (detect_leaks=0); não se afirma ausência de todos os vazamentos. Saída em SANITIZERS.log.

## Interface e pacote

As seis abas foram renderizadas em 1400×860 e 1100×680. Configuração, editor, compilador/simulador de bytecode e importação CPR foram inspecionados visualmente; capturas em screenshots/. A paleta anterior foi preservada. Avisos do plugin Qt offscreen sobre propagateSizeHints não correspondem a falhas dos testes.

Fontes, helper/runtime Linux, manual e firmwares acompanham o pacote. SHA256SUMS cobre o conteúdo; PACOTE.log registra extração, conferência dos hashes e inicialização/compilação em diretório separado. Qt 6.11/glibc 2.42 continuam necessários para o binário GUI desta estação. Este pacote não é um AppImage universal.

## Limites da evidência

A equivalência foi comprovada somente nos casos comparados, sem validação de bytecode no equipamento. Firmware v3.0f é a autorização de escrita suportada; as constantes do compilador são as tabelas offline da interface 3.3. Não foram testados EEPROM após desligamento, movimento, watchdog/parada física, calibração ou gravação USB real. CAN usa codecs e não barramento físico nesta validação.

Firmware serial/CAN/auxiliar, auto PI/FOC/Vk/Kt, caracterização, linearidade, transporte RoboCAN, PDO/escrita CAN, oito sinais arbitrários, TELS/DOA/TID e dump completo permanecem pendentes. docs/ENGENHARIA-REVERSA.md distingue o que foi recuperado e integrado do material apenas identificado.
