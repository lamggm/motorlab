# Perfil preservado: Kalatec KTC-86BLS98-J/X046

Arquivado em 08/10/2026, antes dos testes do KTC-86BLS125-J. Motor antigo de 400 W no canal 1 do SBLG2360T, firmware `Roboteq v3.0f SBLG2XXX 07/15/2026`, encoder AS5047P A/B de 1000 pulsos/volta (4000 contagens/volta).

O [perfil importável](perfil-driver.json) é cópia byte a byte da última exportação verificada, de **08/10/2026 às 10:08:46 no horário de São Paulo**. A origem e os SHA-256 estão no [manifesto](manifesto.json). É uma captura de **636 parâmetros conhecidos de um catálogo de 638**, com valores brutos, escalas e unidades; não é imagem completa da Flash/EEPROM. Não inclui referência mecânica absoluta do encoder incremental, programa MicroBasic ou firmware.

## Configuração final preservada

| Parâmetro | Valor físico ou significado | Registro bruto |
|---|---|---|
| Modo do Motor 1 | Posição por contagem | `MMOD 1 = 3` |
| Velocidade máxima nominal | 3000 RPM | `MXRPM 1 = 3000` |
| Velocidade da trajetória de posição | 30 RPM | `MVEL 1 = 30` |
| Limite / corrente nominal | 11,5 A RMS | `ALIM 1 = NOMA 1 = 115` |
| Aceleração | 3000 RPM/s | `MAC 1 = 30000` |
| Desaceleração normal | 2000 RPM/s | `MDEC 1 = 20000` |
| Encoder | 1000 PPR, feedback do canal 1 | `EPPR 1 = 1000`, `EMOD 1 = 18`, `BLFB 1 = 0` |
| Pares de polos | 4 (8 polos) | `BPOL 1 = 4` |
| PI de velocidade | Kp 0,12; Ki 0,03 | `KPG 1 = 120000`, `KIG 1 = 30000` |
| PID de posição | Kp 0,2; Ki/Kd zero | `KPG 3 = 200000`, `KIG 3 = KDG 3 = 0` |
| PI de corrente, fluxo e torque M1 | Kp 0,2; Ki 50,2656 | `KPF 1/3 = 2000`, `KIF 1/3 = 502656` |
| Watchdog | 1000 ms | `RWD = 1000` |
| Temperatura do controlador | Limite 85 °C | `OTL 1 = 85` |
| Termistor do motor | Não configurado | `R25 1 = B25 1 = 0` |

A tabela Hall e todos os demais registros conhecidos estão no JSON. Resistência/indutâncias `RS/LD/LQ` permaneceram zeradas por falta de caracterização; não substituí-las por dados de outro motor. Os valores são a configuração realmente lida, não uma ficha técnica idealizada.

## Evidência e alcance

[Salvar EEPROM](gravacao-eeprom.json) recebeu ACK; a [releitura posterior](releitura-pos-gravacao.json) confirmou os valores. [Comparação da RAM](comparacao-ram.json) comprovou que somente MXRPM1 e NOMA1 mudaram na rodada de limites nominais. Não houve ciclo de energia depois dessa gravação, portanto retenção após reinício não foi testada nessa rodada. [Estado final](limites-e-estado-final.json): motor parado, encoder e Hall zero, sem novo ensaio de movimento.

Histórico de comportamento deste motor: [sintonia aceita a 20 RPM](../../../SINTONIA-20RPM.md), [ensaio de posição](../../../CONTROLE-POSICAO.md) e [meta experimental de baixa velocidade](../../../BAIXA-VELOCIDADE.md). O ensaio CLI de 500 RPM de 08/10/2026, em modo velocidade, mediu 500,0009 RPM pelo contador, média instantânea encoder de 499,53 RPM, Hall de 501,87 RPM e pico amostrado de 0,9 A, sem falhas nas capturas. Esse ensaio precede o perfil final em MMOD3; não significa que o modo posição e a velocidade foram testados simultaneamente. Não qualifica operação sob carga nem temperatura do motor.

Também foi preservado o [perfil completo em velocidade, testado em 07/10/2026](perfil-velocidade-testado-2026-10-07.json): MMOD1=1, MXRPM1=3000 e os ganhos aprovados a 20–3000 RPM. É um registro histórico da RAM, anterior à seleção de posição e à gravação nominal de 08/10; não apresentá-lo como a última configuração EEPROM.

## Recuperação

1. Voltar a conectar o **mesmo KTC-86BLS98-J com o mesmo conjunto Hall/encoder e fiação**, com o motor parado. Identificar modelo/firmware e exportar o estado atual antes de substituir parâmetros.
2. Abrir `perfil-driver.json` em **Carregar perfil** na GUI, ou usar `motorlabctl call profile` com `action="import_json"` e caminho absoluto. A importação cria somente rascunhos locais.
3. Revisar as diferenças, incluindo parâmetros globais e do Motor 2. Aplicar somente o conjunto necessário na RAM e conferir por releitura.
4. Em posição, preparar referência/destino com eixo parado; A/B não recupera posição absoluta após desligamento. Testar o comportamento antes de salvar na EEPROM.

Modelo, firmware e motor devem corresponder ao conjunto documentado. O fato de o mesmo Roboteq aceitar o arquivo não demonstra que a calibração serve para outro motor.

Fonte nominal do motor: [Kalatec KTC-86BLS98-J](https://www.kalatec.com.br/motor-bldc-400w/).
