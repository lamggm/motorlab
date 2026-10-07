# MotorLab Linux 0.6.0

Aplicativo nativo Linux, com interface C++/Qt 6 e compilador .NET executado no Linux, para **SBLG2360T e SBLMG2360T**. Independente e não oficial da Roboteq. A organização segue as referências do RoboRun+ fornecidas pelo usuário, mantendo as cores do MotorLab. Dois canais; variantes TS não recebem autorização de escrita.

## Checkout Git

Este repositório contém os fontes, testes, documentação e os insumos de compatibilidade descritos em [THIRD_PARTY.md](THIRD_PARTY.md). Os executáveis gerados, o runtime .NET autocontido, caches, logs e registros particulares da bancada permanecem locais. A seção **Abrir** abaixo descreve o pacote binário; para uma cópia recém-clonada, compile primeiro:

```sh
git clone git@github.com:lamggm/motorlab.git
cd motorlab
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
cmake --install build --prefix .
./tools/build_compiler.sh /caminho/absoluto/para/dotnet
ctest --test-dir build --output-on-failure
./iniciar.sh --demo
```

Dependências: compilador C++17, CMake ≥3.20, Qt 6 ≥6.4 (Core, Widgets, Network e Test), Python 3 e SDK .NET 8 para gerar o helper MicroBasic. O SDK deve estar disponível no caminho passado ao script; o runtime resultante fica em `libexec/compiler/`. `compiler/Irony.dll` é um insumo de compilação versionado, distinto dos binários gerados ignorados pelo Git. Não há instalação automática de dependências.

Os relatos de `VALIDACAO.md` registram ensaios históricos. Seus caminhos `../...` e logs referem-se a evidências locais da bancada, que não acompanham o checkout. Configurações de motor desses relatos não são aplicadas ao abrir o programa.

## Abrir

O pacote contém o executável Linux x86_64 e os fontes completos.

```sh
./iniciar.sh
```

Inicia desconectado. Para experimentar sem hardware:

```sh
./iniciar.sh --demo
```

O binário desta estação exige **Qt 6.11, glibc 2.42 e suas dependências dinâmicas**. Não é um AppImage universal; outras distribuições podem compilar os fontes. Não usa Wine, Windows, navegador nem servidor remoto. `dfu-util` é necessário somente para atualizar firmware por USB; nenhuma instalação ou alteração de sistema é feita pelo programa.

## MCP e automação

A versão 0.6.0 inclui **25 ferramentas MCP** ligadas à mesma instância Qt: parâmetros, perfis, telemetria, scripts, manutenção, CANopen de leitura e firmware USB existente. `./iniciar.sh` mostra essa instância compartilhada. O MCP inicia desconectado e identifica explicitamente funções ainda indisponíveis.

O contínuo supervisionado aceita `mode="encoder_speed"` para S/RPM com validação de configuração e limites. Ensaios sem carga a 100 e 3000 RPM realizados em SBLG2360T; detalhes e limitações em `VALIDACAO.md`. Sintonia automática permanece pendente.
A opção `capture_fast=true` registra até 2000 ciclos completos em `job.samples`, incluindo erro, referência da rampa, PWM e correntes FOC Id/Iq. O intervalo entre consultas é reduzido, mas a taxa depende da comunicação: cerca de 38–40 ciclos/s na bancada USB. As consultas não são simultâneas. Essa opção interrompe o ensaio por LoopErr ou corrente FOC acima da guarda. O painel Falhas também mostra erros específicos dos motores quando FF=0; AmpLim aparece como limitação de corrente.


Veja [configuração, ferramentas e limites do MCP](docs/MCP.md). O adaptador requer Python 3 padrão, sem pacotes adicionais.

## Recursos e cobertura

| Área | Disponível nesta versão | Limite atual |
|---|---|---|
| Interface | Cabeçalho de conexão/script, seis abas, duas árvores de configuração em abas internas, ações, monitor, console, gráfico e editor | Interface independente; nomes técnicos e comandos do manual |
| USB / RS232 | Transporte ASCII POSIX, 8N1, seleção manual de porta/baud; identidade FID/TRN | Sem reconexão automática; bancada SBLG2360T v3.0f validada, demais unidades não qualificadas |
| Configuração | **638 campos numéricos**, 155 definições, 153 definições editáveis; encoders, SSI, entradas, CAN/Modbus, motores, PID e FOC | Disponibilidade depende de cada resposta; TELS textual e parâmetros não documentados fora do catálogo |
| Aplicação na RAM | Revisão, pausa do script, parada confirmada por FF, ACK e releitura de cada alteração | Não há transação/rollback automático; mudança de comunicação/segurança deve ser aplicada isoladamente |
| Perfis JSON | Salvar, validar e importar como rascunho; modelo, firmware, origem, unidades e valores brutos preservados | Cobrem o catálogo lido, não toda a memória; CPR disponível por ações separadas |
| EEPROM e proteção | Salvar/carregar EEPROM, restaurar padrões, salvar calibrações, bloquear/desbloquear | Operações explícitas e revisadas; confirmação por ACK, não prova de retenção após desligar |
| Motores | G, S, P e GIQ nos dois canais; slider mantido em G/S/GIQ, parada zero por canal e EX | Exigem habilitação e watchdog lido; adequação ao motor/modo precisa de teste em bancada |
| Console | Consultas, comandos documentados validados e log TX/RX exportável | Não aceita bytes arbitrários/injeção; alterações passam pelas permissões e revisão |
| Telemetria | Corrente, RPM, tensão, temperatura, contadores, FF/FS/FM, 10 entradas digitais, 8 analógicas, 8 pulsos e 4 saídas digitais | Consultas sucessivas, não amostragem simultânea |
| Diagnóstico | Captura contínua documentada, gráfico com janela de até 60 s, valores/extremos e CSV | Taxa solicitada depende do controlador/transporte; não é um osciloscópio |
| MicroBasic | Compilador e interpretador recuperados do original 3.3; HEX, listagem, CRC, simulação de bytecode; depurador de fonte parcial | Constantes offline 3.3; execução física ainda não validada |
| Firmware | Fluxo USB DFU nativo com imagem oficial, CRC, SHA-256, modelo e comparação da Flash por leitura | Experimental, sem teste físico; serial/CAN ainda pendentes |
| CANopen | Inspetor nativo SocketCAN: leituras SDO expeditas e segmentadas, abortos e timeout | Sem escrita, movimento, atualização, PDO/RoboCAN ou identificação exata automática do modelo |
| Motor / sensores | Rotinas documentadas CLMOD dos dois canais, cancelamento e gravação separada de calibrações | Caracterização, correção de linearidade e ajuste automático PI ainda pendentes |

**Novo na 0.4:** análise do instalador RoboRun+ 3.3 fornecido pelo usuário, compilação/exportação HEX, simulação do bytecode original, exportação de roteiro RoboCAN, árvores de parâmetros recuperadas, opções textuais, leitura/comparação/exportação CPR. Relatório técnico em `docs/ENGENHARIA-REVERSA.md`.

DOA e TID permanecem somente para leitura; TELS textual é preservado na leitura/comparação CPR, mas não é aplicado ao controlador. As árvores v3.0 esclareceram ISM (0, 2, 4), B25, R25, FET, FEW, SED, MXPW e ZSMA. SLL foi recuperado apesar de um erro tipográfico no manual. Campos não suportados pela unidade ficam indisponíveis; os padrões das fotos/árvores não são aplicados automaticamente.

A polaridade DINL agora usa entrada 1–10 e nível 0/1; EE oferece 32 posições. Escalas de LD/LQ, VK, FWVR, JR, BR e MXTRN foram alinhadas às árvores originais. Perfis JSON antigos com DINL sem índice são recusados: releia a configuração ou importe o CPR, sem converter automaticamente o valor antigo.

### Perfis RoboRun `.cpr`

**Abrir RoboRun .cpr** exibe todos os parâmetros e identifica quais podem ser importados como rascunho. Itens sem editor, escala incompatível ou fora de limites são listados como não importados. A importação é local; aplicar na RAM continua sendo uma operação separada. Só aceita a família `v3.0` e o modelo selecionado para importar. Aliases de FOC com valores iguais são aceitos; conflitos são recusados.

**Comparar .cpr** compara dois arquivos por comando/índice, valor, tipo e multiplicador. **Exportar .cpr parcial** grava valores conhecidos/rascunhos dos parâmetros mapeados na árvore original. Não é backup completo do controlador. Perfis demo são marcados e bloqueados na importação para unidade real.

## Primeiro uso na bancada

1. Reserve a conexão de bancada e feche outros programas que usem a mesma porta. O aplicativo usa `flock` e TIOCEXCL; isso impede novas aberturas, mas não identifica todos os processos que já tinham a porta aberta sem bloqueio cooperativo.
2. Escolha **USB / Serial**, a porta — preferencialmente `/dev/serial/by-id/...` — e o baud da unidade, normalmente 115200. Clique em **Conectar**. São consultados somente FID e TRN na identificação; nenhuma configuração ou comando de motor é escrito automaticamente.
3. Confira **Detectado: SBLG2360T** ou **SBLMG2360T** e o firmware respondido. Faça inicialmente leituras na aba Monitoramento e **Ler do controlador** na Configuração. Exporte o log, CSV e perfil para registrar o resultado.
4. A edição real de configuração, EEPROM, scripts e motores exige identidade exata e **firmware v3.0f**. Outras versões podem ser lidas; não são liberadas por seleção manual do modelo. O modelo offline serve apenas para rascunhos.
5. Para configurar, marque **Habilitar edição na RAM**, altere os campos e use **Revisar e aplicar na RAM**. O programa pausa o script, aplica EX e exige FF com EStop ativo antes de escrever. Só depois da releitura confirmada considere salvar na EEPROM, por ação separada.

ALIM usa A RMS com escala bruta ×10 e teto de entrada de 45 A. A corrente contínua indicada nos datasheets é 16 A para SBLG2360T e 20 A para SBLMG2360T; o teto do editor não é uma indicação de corrente contínua segura. PID de velocidade/posição usa KPG/KIG/KDG ×1.000.000; fluxo/torque usa KPF/KIF ×10.000. O índice e a página do manual aparecem no tooltip. O painel Ações inicia no modo normal: ler, habilitar edição, aplicar RAM, salvar EEPROM e salvar/abrir perfis. **Modo avançado** revela os recursos RoboRun, recarga da EEPROM, padrões, calibrações, sensores, bloqueio e seleção offline. Alternar o modo só muda a visibilidade. RAM é temporária; EEPROM grava a configuração já aplicada, não os rascunhos da tela; CLSAV salva calibrações na Flash, sem executá-las. A configuração usa abas internas **Entradas / saídas** e **Saída de potência**, com campos de 250 px para novos valores, fontes ampliadas e colunas ajustáveis. A busca expande resultados e muda de aba quando necessário. A roda do mouse apenas rola os painéis: não altera campos numéricos, seletores fechados nem barras de comando, mesmo quando têm foco. Para editar, clique no campo e digite ou use os botões/setas; nas listas, abra e selecione a opção. As unidades físicas são explícitas (ms, Hz, mV, Ω, K, %, PPR e contagens); seletores, máscaras e mapas são identificados como tais. Campos de captura e erro de malha acompanham o PMOD/MMOD lido, e ângulos preservam passos de 1/512 de volta. TID é exibido em A com escala ×10. Os tooltips explicam valores codificados e casos sem conversão física documentada.

Em falha parcial, algumas alterações podem ter sido aplicadas. A edição é revogada e uma nova leitura é necessária. Não há rollback ou reinício de movimento automático. Alterar baud, prioridades, watchdog ou interface pode interromper a própria comunicação. Um lote com essas alterações e outros campos é recusado; aplique-as uma por vez.

## Comandos de motor e parada

A aba Monitoramento mantém movimento desabilitado por padrão. Para liberar comandos físicos, habilite a escrita e **leia RWD configurado em pelo menos 100 ms**; o programa não muda o watchdog ao conectar. Revise o modo MMOD e os limites do motor antes de liberar. G é normalizado ±1000; S é RPM; P é posição absoluta; GIQ é corrente de torque em A ×10, limitado a ±450 nesta aplicação.

**Aplicar** ou mover o slider transmite e mantém G/S/GIQ após soltar o mouse, renovando o comando dentro do RWD. **Parar** e zero enviam zero no modo selecionado (`!S canal 0` em RPM, `!G canal 0` em comando bruto), sem EX e sem revogar a habilitação. Posição absoluta usa Aplicar uma vez e Parar usa MS. O botão vermelho **PARADA !EX** é a emergência. Falha do controlador ou perda de comunicação interrompe o acionamento e registra o motivo. A aba Monitoramento mostra simultaneamente RPM do encoder A/B (`?S`) e Hall/interno (`?BS`), sem substituir uma fonte pela outra. O console também admite PR, GID, D0/D1 e ajustes de contador quando documentados e autorizados.

EX durante uma consulta em andamento é enviado imediatamente e a conexão é encerrada para descartar respostas ambíguas. Nesse caso a mensagem informa que a parada foi solicitada, sem alegar confirmação. Reconecte e confira FF. Fechar normalmente quando armado tenta enviar EX; queda do processo, cabo ou energia do computador não garante parada. Um script autônomo no controlador pode continuar e alimentar o watchdog; EX/RWD do host não substituem a parada física da bancada.

## Captura e registros

A consulta normal percorre 27 comandos em ciclos nominais de 500 ms. A captura contínua usa a formatação documentada do controlador; permite escolher 1, 2, 5, 10, 20, 50, 100 ou 500 ms. RS232 físico exige no mínimo 20 ms nesta versão; USB CDC admite intervalos menores, sujeitos à taxa efetiva. Comece por 50 ms na bancada. A captura contínua inclui A/S/V/T/C/DI/DO/FF/FS; AI, PI e FM não recebem atualizações nessa modalidade e são mostrados indisponíveis.

Gráficos mantêm até 60 s de histórico, com limite de 100.000 pontos. CSV mantém até 36.000 amostras, timestamp do computador em UTC, unidades, identidade e origem demo/serial. Ao atingir o limite, exporte a sessão. Logs são limitados a 2.000 linhas e ocultam as chaves de proteção. As amostras de demo e os perfis dessa origem permanecem identificados após desconectar e não podem ser importados num controlador real.

## Scripts

**Compilar / HEX** usa o núcleo de compilação recuperado de `MicroBasic.dll` do pacote 3.3, portado para .NET 8 no Linux. A janela mostra erros com linha, avisos, tamanho, CRC32 e listagem; permite salvar Intel HEX. Inclui gramática e gerador originais para funções/subrotinas, arrays, macros, condicionais, laços e aliases. O helper e runtime são autocontidos em `libexec/compiler`, sem Wine/WinForms ou instalação global.

**Simular bytecode** executa o interpretador recuperado do original em processo separado: entradas começam em zero, SetConfig guarda valores locais e SetCommand apenas registra eventos. Limites: 100.000 instruções/chamadas, 3 segundos de execução, 64 KiB de saída e 1.000 eventos. Não simula física do motor. A GUI encerra o processo após 8 segundos, inclusive em falha do parser. Fonte limitada a 1 MiB; bytecode a 32 KiB; 1.024 variáveis inteiras e booleanas.

As constantes são as tabelas **offline do RoboRun+ 3.3**; ainda não há download das tabelas da unidade. Compilação equivalente à DLL não comprova adequação a qualquer firmware. O envio e a execução no controlador continuam separados.

**Exportar roteiro RoboCAN** converte o HEX para um arquivo textual com SS/TT simbólicos, handshake, registros, ACKs e commit. Nenhum frame é transmitido. A exportação mantém a parada de emergência ao final, sem emitir liberação nem iniciar o script.

**Depurar fonte** conserva o simulador parcial anterior, com execução por passo, variáveis e entradas configuráveis. Esse depurador não cobre toda a gramática do compilador recuperado; para scripts mais completos use a simulação de bytecode. Exemplo local em `examples/simulacao-local.mbs`.

**Enviar HEX** recebe bytecode previamente compilado para a unidade. Verifica checksum Intel HEX, região contígua 0–32.767, início em zero e EOF; recusa sobreposição, buracos e endereços de firmware. Após pausa/EX/FF, exige HLD e ACK de cada linha. O script não é executado automaticamente. Checksum correto não prova compatibilidade do bytecode; nenhum HEX de teste usado nas suítes deve ser enviado a hardware.

## Atualização USB do firmware

As imagens originais incluídas são **SBLG2XXX v3.0f** e **SBLMG2XXX v3.0f**, de 15/07/2026; os hashes e URLs oficiais estão em `data/firmware.json`. Uma imagem da outra família, TS, alterada ou desconhecida é recusada mesmo que seu CRC seja válido.

O fluxo exige conexão USB CDC identificada, versão v3.0/v3.0a–f, configuração lida sem rascunhos e perfil salvo. A revisão informa quantos dos 638 campos foram lidos: esse perfil não é uma imagem completa da memória. A versão anterior pode ser atualizada sem liberar edição geral. O programa pausa script, exige EX/FF, solicita DFU e fecha a porta serial.

`dfu-util` é chamado diretamente, sem shell, restringindo a operação ao **mesmo caminho físico USB**, dispositivo DFU `0483:df11` e alvo 0. Após gravação, tenta comparar os bytes da Flash por upload. Falha ou timeout não provoca retomada/repetição da gravação. O controlador permanece em DFU; reinicie, reconecte e confira FID antes de usar. Nenhuma atualização física foi realizada nesta entrega; o fluxo depende da validação na bancada. Firmware serial, CANopen, auxiliar, bootloader e recuperação avançada permanecem pendentes.

## CANopen

Abra **Console → CANopen / SocketCAN**, escolha uma interface existente e ativa e o Node ID. O aplicativo não cria a interface, não muda bitrate e não executa sudo, ip ou modprobe. Envia somente requisições SDO de leitura. Decodifica transferências expeditas/segmentadas, toggle, comprimento e abortos, com timeout de 900 ms e limite de 4 KiB por objeto.

Há bloqueio cooperativo por interface/nó entre instâncias deste inspetor. SocketCAN não oferece propriedade exclusiva do barramento: outros clientes precisam ser coordenados externamente. DeviceName e versão genérica do EDS não comprovam o modelo exato, portanto o inspetor não libera escrita nem movimento. Transporte SocketCAN real e firmware CAN ainda não foram testados.

## Compilar e testar

Fontes exigem Linux/POSIX, C++17, CMake ≥3.20, Qt 6 ≥6.4 com Widgets e Qt Test para as suítes. O binário entregue foi compilado em Qt 6.11.2/GCC 16.2.1. Qt SerialPort não é necessário.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/motorlab --demo
```

O helper já está incluído. Para recompilá-lo, forneça um SDK .NET 8:

```sh
./tools/build_compiler.sh /caminho/absoluto/dotnet-sdk/dotnet
python3 tools/test_compiler.py
```

Ao executar o binário dentro de um diretório de build, configure `MOTORLAB_COMPILER` com o caminho absoluto de `libexec/compiler/motorlab-compiler`; o launcher do pacote encontra esse helper automaticamente. CTest já configura o caminho para a suíte GUI.

Para omitir as suítes, adicione `-DBUILD_TESTING=OFF` à configuração. Nenhum teste procura controladores físicos: usam demo, arquivos, codecs ou pseudoterminais locais. Resultado e limites da evidência em `VALIDACAO.md`, com saídas em `TESTES.log` e `SANITIZERS.log`. `SHA256SUMS` cobre o conteúdo entregue.

## Referências

Consultadas em 06/10/2026. O manual RoboRun+ solicitado está incluído em `docs/RoboRun-Plus-User-Manual-v3.2.pdf`.

- [RoboRun+ Utility User Manual v3.2](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/user-manual/1191-roborun-utility-user-manual-v3-2/file), referência das funções e interface.
- [Controllers User Manual v3.0c](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/user-manual/1064-roboteq-controllers-user-manual-v3-0/file), 01/09/2026, protocolo, parâmetros, escalas e comandos.
- [MicroBasic Scripting Manual v2.1a](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/user-manual/1063-microbasic-scripting-manual-v2-1a/file), subconjunto do simulador local.
- [CAN/EtherCAT Networking Manual v3.0d](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/user-manual/1065-can-ethercat-networking-manual-v3-0/file), SDO e objetos.
- [SBLG2360T](https://www.roboteq.com/products/products-brushless-dc-motor-controllers/sblg2360t-393-detail) e [SBLMG2360T](https://www.roboteq.com/products/products-brushless-dc-motor-controllers/sblm2360t-452-detail), famílias e firmware.
- [Datasheet SBLG23xx](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/datasheets/robog4/1060-sblg23xx-datasheet/file) e [SBLMG23xx](https://www.roboteq.com/docman-list/motor-controllers-documents-and-files/documentation/datasheets/robog4/1061-sblmg23xx-datasheet/file), canais, I/O e limites elétricos.
- [Manual dfu-util](https://dfu-util.sourceforge.net/dfu-util.1.html), seleção de caminho, download e upload.

## Pendências para equivalência com RoboRun+

Ainda faltam: parametrização/aplicação de TELS, DOA e TID; backup completo; firmware serial/CAN/auxiliar; CANopen com identidade exata, escrita/PDO/movimento; transporte RoboCAN; captura arbitrária de oito sinais/oscilloscope; caracterização, ajuste automático PI/FOC/Vk/Kt e linearidade; depuração por passos do bytecode com entradas editáveis; consulta das constantes de compilação da unidade. A engenharia reversa identificou componentes dessas funções, mas eles não foram integrados nesta versão. A validação física de comunicação, persistência, watchdog, comando, bytecode, calibração e DFU nos dois modelos continua pendente.

Código MotorLab sob MIT; materiais Roboteq e bibliotecas têm licenças próprias, conforme `THIRD_PARTY.md`.
