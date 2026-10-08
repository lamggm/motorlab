# Primeiro teste: Kalatec KTC-86BLS125-J

Ensaios de 08/10/2026 no Motor 1 do SBLG2360T, firmware v3.0f (15/07/2026), pelo backend CLI nativo e API MCP. O operador confirmou motor novo ligado e fixado, eixo livre e AS5047P instalado. A resolução foi conservada em 1000 PPR / 4000 contagens por volta e verificada pela concordância de sentido/velocidade entre encoder e Hall. A aceitação visual de suavidade permanece pendente.

O [perfil antigo do KTC-86BLS98-J](../../ktc-86bls98-j/2026-10-08/README.md), incluindo calibração e ganhos, foi arquivado e enviado ao GitHub **antes das alterações**. Sua configuração final de posição e o perfil histórico validado de velocidade estão disponíveis separadamente.

## Dados do motor e divergência de corrente

A [página comercial](https://www.kalatec.com.br/motor-brushless-bldc-700w/) usa a URL de 700 W, mas identifica o modelo KTC-86BLS125-J, potência de 660 W, 48 V, 3000 RPM e 8 polos. Informa 6,3 A como corrente nominal. Essa corrente não é coerente com 660 W a 48 V: a entrada elétrica seria apenas 302,4 W.

Os desenhos oficiais [X047, com chaveta](https://www.kalatec.com.br/wp-content/uploads/2020/02/KTC-86BLS125-J-X047%EF%BC%88EIXO12.7mm-com-chaveta.pdf) e [X048, sem chaveta](https://www.kalatec.com.br/wp-content/uploads/2020/02/KTC-86BLS125-J-X048%EF%BC%88EIXO12.7mm.pdf), conferidos visualmente, especificam **19 A nominais**, 660 W, 48 V, 3000 ±10% RPM, torque nominal 2,1 Nm, constante de torque 0,124 Nm/A, força contraeletromotriz 13 V/kRPM e resistência entre fases de 0,13 Ω. Adotados 19 A e 3000 RPM como limites do ensaio, sem liberar a rotação sem carga de 3700 RPM como limite de operação.

A resistência entre fases não foi inserida diretamente como resistência de estator Rs; LD/LQ também não foram estimados a partir da faixa comercial de indutância. RS/LD/LQ permanecem sem caracterização. A temperatura ambiente da ficha não virou limite de temperatura do motor.

## Preparação e configuração na RAM

Estado inicial: barramento 48,3 V, controlador 34 °C, encoder/Hall do Motor 1 parados e FF=0. A GUI foi desconectada, com comandos manuais desabilitados e sem rascunhos, para reservar a USB à CLI. A leitura inicial e final comparou os mesmos 636 parâmetros conhecidos; o Motor 2 e os demais registros comparáveis foram preservados.

Primeiro foi usado MMOD0 com G50 (5% de comando, **não 50 RPM**) durante aproximadamente 8 s, para conferir sentido e escala do encoder antes de fechar a malha. Depois, MMOD1=1 ativou o controle de velocidade com feedback externo por encoder. A **comutação trapezoidal por Hall** foi escolhida para estes testes iniciais, sem usar a calibração sinusoidal do motor antigo. Não houve execução CLMOD, gravação de calibração ou sintonia FOC nesta rodada. A tabela Hall antiga permanece armazenada, mas não é uma calibração validada para o motor novo.

| Parâmetro ativo | Valor | Registro bruto |
|---|---:|---|
| Modo de controle | Velocidade por encoder | `MMOD 1 = 1`, `BLFB 1 = 0` |
| Comutação | Trapezoidal / Hall | `BMOD 1 = 0`, `BFBK 1 = 1` |
| Corrente máxima e nominal | 19 A RMS | `ALIM 1 = NOMA 1 = 190` |
| Velocidade máxima | 3000 RPM | `MXRPM 1 = 3000` |
| Potência nominal registrada | 660 W | `MXPW 1 = 6600` |
| Constante de torque | 0,124 Nm/A | `TNM 1 = 124` |
| Constante de tensão | 13 V/kRPM | `VK 1 = 13000` |
| Motor / encoder | 4 pares de polos, 1000 PPR | `BPOL 1 = 4`, `EPPR 1 = 1000`, `EMOD 1 = 18` |
| PI inicial de velocidade | Kp 0,03; Ki 0,01; Kd 0 | `KPG 1 = 30000`, `KIG 1 = 10000`, `KDG 1 = 0` |
| Aceleração / desaceleração normal | 3000 / 2000 RPM/s | `MAC 1 = 30000`, `MDEC 1 = 20000` |
| Corrente de busca, para futura calibração | 19 A | `BZPW 1 = 190` |
| Proteção térmica do controlador | 85 °C, preservada | `OTL 1 = 85` |

Os ganhos iniciais de velocidade foram reduzidos em relação ao motor anterior para validar o conjunto novo. Não são a sintonia final. Ganhos de corrente e posição anteriores foram preservados no perfil completo, mas não foram qualificados para uso sinusoidal/posição no motor novo.

O supervisor recebeu explicitamente `max_current_a=19`, `max_speed_rpm=3000`, `max_power_percent=100`, `max_temperature_c=85`, lease de 3 s renovada por ciclos novos e watchdog de 1000 ms. O esquema aceita corrente decimal e até 19 A, enquanto a guarda padrão de 11,5 A ainda corresponde ao motor antigo; para este motor, informar 19 A explicitamente. Não houve redução de MXRPM para os alvos de 100 ou 500 RPM.

## Resultados medidos

Médias pelo deslocamento do contador A/B, com 4000 contagens/volta, depois dos primeiros 3 s. As faixas são leituras instantâneas na mesma região. O último trecho de 5 s exclui uma parcela maior do assentamento inicial.

| Ensaio | Média pelo contador | Encoder instantâneo | Média Hall | Pico de corrente amostrado |
|---|---:|---:|---:|---:|
| G50, validação inicial | 155,34 RPM | 134–173 RPM | 157,36 RPM | 2,2 A |
| Alvo 100 RPM, 10 s | 97,43 RPM | 63–130 RPM | 98,60 RPM | 1,6 A |
| Alvo 500 RPM, 20 s | 497,81 RPM | 482–511 RPM | 499,74 RPM | 2,0 A |

Nos últimos 5 s: **98,47 RPM** no alvo de 100 e **499,91 RPM** no alvo de 500. A aproximação lenta do alvo e a dispersão, sobretudo a 100 RPM, mostram que a sintonia inicial ainda pode ser melhorada. A 500 RPM, desvio padrão instantâneo do encoder de 5,04 RPM após 3 s. Não atribuir toda a variação à oscilação mecânica: consultas são sequenciais, e a análise não substitui observação do eixo ou medição independente.

Capturas: 369, 310 e 625 ciclos completos, respectivamente. FF/FM1 ficaram zero nas três capturas. Cada ensaio terminou por EX com confirmação FF16; FM1=128 no estado parado decorre dos transistores desligados pela parada. Motor 1 final parado por encoder e Hall. O encoder do canal 2 mostrou pequenas leituras residuais (-1/-2 RPM) com Hall, comando e potência do canal 2 zero; não foi usado como feedback destes ensaios.

Não foram testados nesta rodada: 20 RPM, reversão, 3000 RPM, controle de posição, comutação sinusoidal calibrada, carga/redutor/roda ou regime térmico prolongado. Não há monitoramento de temperatura do motor confirmado.

## Arquivos e estado persistente

- [Perfil final, somente RAM](perfil-driver-ram.json)
- [Comparação completa dos valores conhecidos](comparacao-parametros.json)
- [Validação Hall/encoder](hall-encoder-inicial.json) e [captura CSV](hall-encoder-inicial.csv)
- [Teste 100 RPM](velocidade-100rpm.json) e [captura CSV](velocidade-100rpm.csv)
- [Teste 500 RPM](velocidade-500rpm.json) e [captura CSV](velocidade-500rpm.csv)
- [Proveniência e SHA-256](manifesto.json)

**Estado no fim dos primeiros ensaios:** somente RAM, sem EESAV/CLSAV. **Atualização posterior autorizada:** o perfil novo foi duplicado para o Motor 2 e salvo por EESAV; ver [configuração dos dois motores e evidências](dois-motores-eeprom/README.md). A EEPROM antiga foi substituída e seu backup continua no Git. Não houve ciclo de alimentação para validar a recuperação após reinício. Importar o JSON cria rascunhos; aplicar na RAM e salvar EEPROM continuam operações separadas após revisão e validação do motor conectado. Não habilitar modo sinusoidal usando a calibração antiga como se fosse do novo motor.
