# RoboRun+ 3.3 → MotorLab Linux 0.4

Análise local de 07/10/2026 do `RoborunPlus v3.3 20260202.zip` fornecido pelo usuário. SHA-256: `bcaeabb70d9d1ff346e0996571d930389fd37038f669e8c9b55de613c0e0b58d`. Instaladores, interfaces WinForms e ferramentas Windows de gravação não foram executados. A DLL MicroBasic original foi chamada apenas como oráculo de testes no runtime Linux, conforme descrito abaixo. Nenhum controlador foi conectado por este trabalho.

## Extração e evidência

O ZIP contém um MSI e bootstrapper. O gabinete do MSI contém 2.757 arquivos: 2.621 árvores `.ctree`, 36 perfis `.cpr`, bibliotecas gerenciadas, executáveis e scripts. As tabelas File/Component/Directory e o pool de strings do MSI foram decodificados para reconstruir os caminhos originais — inclusive as versões de firmware. Não se escolheu uma árvore apenas pelo nome do arquivo repetido.

ILSpyCMD 9.1.0.7988 foi executado com .NET 8 local ao workspace. Foram inspecionados MicroBasic.dll, Controller.dll, TreeDesigner.dll, RoborunPlus.exe, Roboteq.Controllers.dll e Utility/FirmwareLoader/FirmwareLoader.exe. O runtime/SDK baixado da Microsoft teve SHA-512 conferido. Nenhuma instalação global, configuração de shell, serviço ou driver foi alterado.

A pasta de análise preservada no workspace é `work/reverse-v3.3/`: `filemap.json`, `payload/`, `decompiled/`, `oracle/` e logs. `docs/roborun-3.3-provenance.json` registra os hashes dos principais arquivos utilizados. Os blobs de ícones BinaryFormatter embutidos nas árvores não foram desserializados.

## Funções recuperadas e integradas

| Origem | Evidência | Resultado no Linux |
|---|---|---|
| MicroBasic.dll, namespaces Compiler / Common / CompilerUnits / Tokens | Gramática Irony, tabela de opcodes, emissão de bytecode e CRC32 | Compilação nativa no runtime Linux, listagem, erros/avisos, exportação Intel HEX |
| MicroBasic.dll, ByteCodeInterpreter | Interpretador, temporizadores, pilhas e interface Get/Set | Simulação do bytecode em processo separado; Get/Set exclusivamente locais |
| Controller.dll, CompilerConstantGetSetConfig / SetCommand / GetValue | Tabelas offline usadas por InitConstantList | Constantes e aliases originais, identificados como offline 3.3 |
| MicrobasicHelper.ConvertTokens | Registros de 16 bytes, endereço crescente, preenchimento FF, checksum e EOF | HEX com o mesmo formato do original |
| TreeDesigner, ScriptHelper.ConvertMicrobasicToCAN | Pedido, handshake, fragmentos de 4 bytes, ACK e commit | Exportação textual de roteiro RoboCAN; sem transmissão e sem liberação de EX ao terminar |
| Trees/v3.0/SBLG2360T + RCBSBLG2XXXT | 617 nós de parâmetro, incluindo aliases | Categorias, descrições e opções da configuração |
| Trees/v3.0/SBLMG2360T + RCBSBLMG2XXXT | 623 nós de parâmetro, incluindo aliases | Categorias comuns e metadados específicos preservados |
| ControllerProfile / ControllerProfileItem e arquivo CPR original | XML v1.0, valores brutos, multiplicador, tipo, índice e caminho | Leitura, comparação, importação de rascunhos compatíveis e exportação parcial |

O compilador contém 60 arquivos C# derivados do núcleo/tabelas originais, separados em `compiler/vendor/`. O adaptador de processo e limites está em `compiler/Program.cs`. O programa principal continua C++/Qt. Não há Wine, WinForms nem chamadas ao controlador dentro do helper.

## Correções fundamentadas no pacote

- ISM: opções 0, 2 e 4. Os índices 1 e 3 são vazios e recusados.
- B25: −65000…65000; R25: 0…200000.
- FET e FEW: 1…30000; SED: 0…2; ZSMA: −32768…32767.
- MXPW: 0…500000 bruto, multiplicador 10, unidade W.
- SLL: comando presente nas árvores e constantes. O manual v3.0c escreve `SELL` no exemplo de sintaxe, mas a consulta, nome, exemplo e objeto CAN 0x30D7 identificam SLL. Incluído com dois índices e inteiro assinado de 32 bits.
- DINL corrigido para 10 entradas com valor 0/1, conforme a árvore e o exemplo/sintaxe do manual (o parágrafo e o número de argumentos antigos são contraditórios). A forma escalar em bitmap da versão anterior foi removida.
- EE ampliado para 32 posições de −32768…32767, conforme ambas as árvores v3.0.
- Escalas de apresentação alinhadas ao RoboRun: LD/LQ ×1000 em mH, VK ×1000 em V/kRPM, FWVR ×10 em %, JR ×1000, BR ×10000 e MXTRN ×100 em voltas. ATGA=0 (No Action) agora é aceito.
- Listas predefinidas TYP vêm de NodeConfigHelper.Lookups; entradas vazias ou prefixadas por `~` são omitidas/recusadas como no NodeComboBox original.
- O CPR original contém aliases KPF/KIF em dois caminhos, com valores iguais. O parser preserva os caminhos e rejeita aliases conflitantes; a importação aplica um único rascunho por comando/índice.
- `CheckComboBox` representa combinações de bits e não foi tratado como enumeração simples. Somente `ComboBox` usa a lista de opções numéricas simples.

A versão da interface 3.3 não é uma autorização para escrever em firmware 3.3. As árvores integradas são v3.0 para os dois modelos e a autorização de escrita real continua limitada ao firmware v3.0f já suportado. Valores padrão das árvores não são enviados automaticamente.

## Verificação

Uma segunda aplicação de teste chama a **DLL MicroBasic.dll original e inalterada** no runtime Linux. Foram comparados bytecode, HEX, tamanho, CRC32, avisos e erros com o helper recompilado: oito casos sintéticos e seis scripts do instalador, todos equivalentes. Os seis scripts originais geraram entre 627 e 993 bytes. Isso comprova equivalência nesses casos, não toda entrada possível ou execução física.

`tools/test_compiler.py` reproduz os testes funcionais locais, simulação e limites; `--oracle DOTNET ORACLE_DLL` habilita a comparação com o harness original preservado no workspace. O harness precisa da DLL original fornecida pelo usuário. Ele não é usado pelo aplicativo entregue.

O núcleo Qt testa o round-trip do CPR original (613 itens, incluindo aliases), XML incompleto, DTD, duplicatas, conflitos, índices/inteiros inválidos, opções ISM e roteiro RoboCAN. A GUI verifica compilação/simulação sem escrita no pseudoterminal, estrutura das árvores e exportação CPR com escala/origem. Resultados consolidados em `VALIDACAO.md`.

## Limites e trabalho restante

O CPR é carregado integralmente para leitura/comparação, mas a importação oferece apenas itens com editor e escala/limites compatíveis. DOA e TID continuam somente para leitura; TELS textual não é aplicado. A exportação CPR é parcial e identificada como tal. Não existe dump completo da memória.

A extração também encontrou rotinas de caracterização, correção de linearidade, auto PI/FOC, ajuste de Vk/Kt, flasher serial e carregadores auxiliares. Esses componentes possuem estados, dependências de firmware, alterações temporárias e sequências físicas que ainda não foram portados. Firmware serial/CAN/auxiliar, transporte RoboCAN, CANopen de escrita/PDO/movimento, osciloscópio de oito sinais e consulta das constantes da unidade permanecem pendentes.

A simulação de bytecode tem entradas em zero, limite de execução e não representa física do motor. O depurador por passos da fonte continua sendo o subconjunto independente anterior. Compilar/exportar não envia nem executa o script; a carga HEX e Run são ações separadas. Nenhuma função física foi validada em bancada nesta entrega.

Os materiais derivados da Roboteq não receberam licença MIT. Consulte `THIRD_PARTY.md`; nenhum pacote ou fonte foi publicado em serviço externo.
