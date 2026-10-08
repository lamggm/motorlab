# Biblioteca de perfis de motor

Com o controlador conectado, use **Ler do controlador** antes de preparar um perfil (o diálogo também oferece **Ler controlador…**). Em **Configuração → Perfis de motor…**, escolha um perfil e o destino **Motor 1** ou **Motor 2**. A prévia apresenta o valor lido do canal e o valor do perfil, em unidades físicas e com os nomes em português. O nome original e a descrição aparecem no tooltip.

**Preparar alterações** cria rascunhos no backend compartilhado, sem enviar comandos ao driver. Encerre o controle manual ou outra operação antes de preparar. Revise os campos e use **Aplicar alterações (RAM)**. Essa ação passa pela revisão, ACK e releitura existentes. **Salvar permanente (EEPROM)** conserva a configuração aplicada após desligar. Preparar, importar, exportar e salvar um perfil no computador não gravam RAM nem EEPROM.

## Perfis incluídos

| Perfil | Registro de origem | Ajustes principais |
|---|---|---|
| Atual: Kalatec KTC-86BLS125-J, 660 W, velocidade | `perfis/ktc-86bls125-j/2026-10-08/dois-motores-eeprom/perfil-final-eeprom.json` | 3000 RPM, 19 A, quatro pares de polos, encoder 1000 PPR, comutação trapezoidal Hall, PI inicial 0,03/0,01 |
| Antigo: Kalatec KTC-86BLS98-J/X046, 400 W, velocidade | `perfis/ktc-86bls98-j/2026-10-08/perfil-velocidade-testado-2026-10-07.json` | 3000 RPM, 11,5 A, encoder 1000 PPR, senoide, PI de velocidade 0,12/0,03 |
| Antigo: Kalatec KTC-86BLS98-J/X046, 400 W, posição | `perfis/ktc-86bls98-j/2026-10-08/perfil-driver.json` | Ajustes do registro final de posição, ganho proporcional de posição 0,2 |

Os registros têm proveniência e SHA-256 embutidos em `data/motor-profiles.json`. São configurações de bancada preservadas, não uma nova qualificação física: o conjunto atual foi ensaiado separadamente a 100 e 500 RPM sem carga. Os limites nominais não comprovam funcionamento simultâneo, sob carga ou térmico a 3000 RPM.

## Canais e calibração

O perfil transporta 90 campos do motor e do encoder associado. Por padrão, prepara **72 campos**, excluindo os 18 registros de calibração física (`HSAT`, `ZSMC`, `ZSMA`, `HSM`, `BADJ`). A opção de incluir calibração é destinada ao mesmo conjunto físico de motor e sensores já calibrado. A calibração depende de montagem e alinhamento; não se transfere automaticamente para outro exemplar.

No perfil de 660 W, essa opção fica indisponível: a calibração Hall/SinCos presente no arquivo completo era remanescente do motor antigo e não foi validada para esse motor. Ela não é copiada.

O remapeamento usa os caminhos recuperados do RoboRun. Isso inclui índices específicos de velocidade/posição, fluxo/torque, tabelas Hall, SinCos e temperatura do motor. O limite de temperatura global `OTL 1` fica fora. Encoder 1 (`A1/B1`) acompanha Motor 1 e encoder 2 (`A2/B2`) acompanha Motor 2; a associação `EMOD` é adaptada para o destino. Configurações globais de comunicação, watchdog, STO e o outro canal ficam fora do perfil. Rascunhos preexistentes dos campos selecionados são substituídos; rascunhos fora desses campos são preservados.

Para uma unidade real, o modelo de controlador e a identificação exata de firmware devem coincidir. Perfis de demonstração são recusados em uma unidade real.

## Salvar outros motores

Leia a configuração completa do controlador e aplique ou descarte os rascunhos do canal antes de usar **Salvar este canal como perfil…**. Informe um nome que identifique motor e ajuste, por exemplo “Motor da esteira — velocidade”. O perfil é criado a partir dos valores lidos, não dos rascunhos ainda não aplicados.

Perfis pessoais persistem no diretório `motor-profiles` de `QStandardPaths::AppDataLocation` (normalmente `~/.local/share/MotorLab Linux/motor-profiles`). Os perfis incluídos ficam embutidos no aplicativo e não podem ser excluídos. É possível exportar um perfil, importar um arquivo válido como cópia pessoal e excluir perfis pessoais. Arquivos pessoais são gravados atomicamente.

**Salvar backup / Abrir backup**, na seção Backup do controlador, continuam trabalhando com o catálogo completo do driver. Esses backups são diferentes dos perfis transferíveis de um motor.

## Backend e validação

A GUI envia o plano validado ao `motorlab_stage` existente no daemon; não abre a serial nem escreve parâmetros por conta própria. A CLI/MCP vê os mesmos rascunhos. A biblioteca e o remapeamento vivem em `motorlab_core`; não exigem migração de estado nem reinício do daemon.

Testes automatizados verificam proveniência dos três registros, exclusão de parâmetros globais, remapeamento para Motor 2, calibração opcional, validação de modelo/firmware/valores, persistência, importação local e compartilhamento dos rascunhos sem comandos de movimento ou gravação em um controlador simulado.
