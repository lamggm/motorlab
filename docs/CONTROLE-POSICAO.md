# Controle de posição — bancada de 07/10/2026

Teste autorizado pelo operador no Motor 1 SBLG2360T, firmware v3.0f, motor Kalatec KTC-86BLS98-J/X046 sem carga. AS5047P em A/B, 1000 pulsos/volta configurados: 4000 contagens/volta e 0,09°/contagem. Os ângulos abaixo são calculados pelo próprio encoder; não houve medição angular externa.

## Resultado

| Ensaio | Alvo relativo à referência temporária | Última posição energizada | Erro final | Pico de velocidade medido |
|---|---:|---:|---:|---:|
| Avanço | 1000 contagens / 90° | 999 / 89,91° | −0,09° | 44 RPM |
| Retorno | 0 / 0° | 4 / 0,36° | +0,36° | 43 RPM |

Cada captura durou aproximadamente 8 s. No avanço, o último segundo permaneceu em 999 contagens; no retorno, variou entre 5 e 4. Corrente de motor máxima amostrada: 0,9 A no avanço e 1,0 A no retorno; Iq máximo 1,4 A e 1,2 A. FF e FM1 permaneceram zero nas capturas. O operador confirmou: “Sim, avançou e ficou parado”, em resposta à pergunta sobre avanço próximo de 90° sem vibração ou trancos.

O avanço entrou e permaneceu na faixa de ±5 contagens em 0,649 s. O retorno levou 6,339 s para essa mesma faixa, embora já estivesse dentro de ±12 contagens em 0,714 s. Portanto, a aproximação fina na reversão ainda é lenta. O ensaio de avanço começou em 190 contagens após o ensaio preliminar; 90° é o destino desde o zero temporário, não o deslocamento dessa captura isolada.

Esses números descrevem retenção com torque ativo. Depois de EX no retorno, a leitura passou de 4 para 12 contagens (1,08° da referência); desligar os transistores não mantém a posição. A captura usa consultas sequenciais, aproximadamente 28 ciclos/s, e não exclui picos entre amostras. Não valida carga, repetibilidade de muitos ciclos ou exatidão mecânica absoluta.

## Configuração testada e estado final

No teste: MMOD1=3 (Count Position), MVEL1=30 RPM, MXRPM1=100 RPM como limite temporário, ganho proporcional de posição KPG3=0,2, integral e derivativo de posição zero. O primeiro teste com KPG3=0,1 aproximou-se lentamente; somente esse ganho de posição foi aumentado. PI de velocidade 0,12/0,03, ganhos de corrente e calibração Hall anteriores foram preservados. Aceleração 3000 RPM/s e desaceleração 2000 RPM/s não foram alteradas.

Ao terminar, todos os **636 parâmetros conhecidos foram restaurados e relidos sem diferenças** em relação ao perfil anterior: MMOD1=1, MXRPM1=3000, MVEL1=100 e KPG3=0,1. O ajuste experimental KPG3=0,2 está no arquivo `perfil-posicao-testado.json`, não está ativo. Não houve gravação EEPROM nesta rodada. Motor final parado, encoder e Hall 0 RPM, FF16, FM1=128, permissão de movimento desligada.

A referência anterior do contador era 67457487. Ela foi restaurada somando o deslocamento físico final de 12 contagens: contador final 67457499. A alteração de referência foi feita apenas com ambos os canais em malha aberta, EX confirmado e velocidades zero. Não houve liberação de movimento durante essa restauração.

O perfil de posição é registro do ensaio, não uma receita de ativação direta: mudar MMOD para posição exige preparar a referência e o destino com o motor parado. A conexão A/B fornece contagem incremental; o teste não implementa homing nem posição absoluta recuperável após desligamento.

## Implementação no MCP

`motorlab_continuous` ganhou `mode="encoder_position"`: alvo absoluto em contagens, inclusive zero. Exige MMOD3, encoder externo associado, MXRPM e MVEL até 100 RPM, motores inicialmente parados, deslocamento inicial até 4000 contagens e janela entre origem/destino com margem de 100 contagens. A guarda de ausência de progresso permite repouso dentro de 20 contagens; isso não é critério de precisão ou sucesso do posicionamento.

O ciclo repete `!G 1 0`, `!S 1 MVEL` e o mesmo `!P 1 destino`. Nunca repete deslocamento relativo `!PR`. Nos ensaios desta unidade, P enviado uma vez não produziu deslocamento; repetir o destino absoluto produziu a trajetória. G0 tornou a fonte serial ativa, mas isoladamente não resolveu. A causa interna exata de firmware/temporização não foi isolada; esta é uma sequência validada empiricamente nesta unidade.

Captura inclui posição real F/C, trajetória TR, velocidade demandada VDV, erro E, corrente Id/Iq e flags. Lease, watchdog, corrente, velocidade, temperatura, LoopErr e janela de posição interrompem por EX. O fim do job por parada confirmada não significa alvo atingido; o relatório compara o contador real. Isso não muda o botão Parar comum da GUI, nem substitui o caminho de posição existente nos sliders.

Manutenção de contadores C/CB exige ambos os canais em MMOD0, velocidades de encoder/Hall zero e releitura exata após escrever. Pausa de script rejeitada só é tolerada com EX/FF/FS confirmando parada e ausência de script/setup.

## Evidências e documentação

Perfis, CSVs dos dois ensaios, resumo numérico e estado final: [pasta de evidências](ensaios/2026-10-07-posicao/). Logs completos da bancada ficam em `outputs/posicao-bancada-2026-10-07/`, fora do repositório.

Referência técnica: manual local de controladores v3.0c, seção Count Position (pp. 193–197), MMOD (pp. 409–410), comandos P/PR/S e consultas TR/VDV; complemento oficial [Closed Loop Count Position G4](https://roboteq.freshdesk.com/support/solutions/articles/70000661205-closed-loop-count-position-g4). A documentação G4 é apoio conceitual; os resultados acima foram medidos no SBLG2360T v3.0f.

## Verificação do software

Build concluído e implantado com controlador parado; recarga preservou os 636 parâmetros conhecidos. Protocolo: 18 testes aprovados. Posição isolada: 11 aprovados, incluindo repouso no alvo, referência zero, limite de percurso, LoopErr e expiração da lease.

Rodada consolidada: **43 aprovações e 1 falha**, abrangendo posição, manutenção de referência, EEPROM, aplicação RAM e regressão de velocidade. A falha foi `encoderSpeedSupervision(rated-positive)`: o simulador fragmenta respostas e a guarda detectou atraso na comunicação, encerrou o job e confirmou EX; o teste esperava parada por operador. Esse cenário passou em execução anterior isolada, mas a intermitência de temporização continua pendente. Não se declara a suíte consolidada integralmente aprovada nem se relaxou o watchdog para fazê-la passar. Logs preservados nesta pasta. Os dois ensaios físicos de posição terminaram com parada confirmada, sem disparo dessa guarda.

## Atualização: posição salva na EEPROM a pedido do operador

Em 07/10/2026, após o pedido explícito para testar manualmente, aplicado o perfil ensaiado e enviado `%EESAV`, com ACK `+` registrado. Antes, motor parado, ambos os canais em MMOD0, referência C1 zerada e relida em zero. Depois: MMOD1=3, MVEL1=30 RPM, MXRPM1=100 RPM, KPG3=0,2, KIG3/KDG3=0. MDEC1=2000 RPM/s, sintonia de velocidade/corrente, calibração e Motor2 preservados. Comparação de636 parâmetros confirmou apenas quatro alterações (MMOD1/MVEL1/MXRPM1/KPG3). Essa atualização substitui o estado final de velocidade relatado no ensaio anterior.

ACK de EEPROM e releitura da RAM confirmados; não houve reinício ou ciclo de energia para validar retenção. Referência incremental zerada vale nesta sessão: a gravação de configuração não transforma o A/B em posição absoluta nem grava um zero mecânico recuperável. Nenhum movimento foi comandado nesta etapa. Final C0, encoder/Hall0RPM, FF16, controle manual desligado.

Interface atualizada e recarregada preservando636 parâmetros. Em P, barra de−4000 a4000contagens; mover escolhe o rascunho, Aplicar confirma o destino. O alvo confirmado é renovado em sequência G0/S=MVEL/P, incluindo destino zero. A barra não altera o alvo já ativo antes de Aplicar; terminar edição do campo numérico também confirma. Requer parâmetros MMOD3/MVEL/MXRPM conhecidos, sem rascunhos nesses campos e velocidades de configuração entre1 e100RPM. Parar usa MS, cancela somente comandos pendentes daquele canal e mantém a autorização manual; não envia P0 nem EX. Um novo destino pode ser aplicado. EX continua separado. Não há mudança automática de MMOD ao escolher P na interface.

Testes de pseudoterminal: UI5/5 (posição mantida, rascunho, alvozero, MS, rejeição e regressão G/S); protocolo19/19, incluindo preservação do canal2 e da autorização ao parar canal1. Essa rodada não moveu o motor para validar fisicamente a nova interface; o operador fará o teste manual solicitado. Evidências em [posição EEPROM](ensaios/2026-10-07-posicao-eeprom/).

## Correção do slider em 08/10/2026

O teste manual registrou `!G 1 475` repetido e aceito, mas sem movimento: MMOD1 estava em 3 (Count Position). Nesse modo, o manual especifica que G não tem efeito. ACK confirma recepção, não deslocamento.

Ao conectar, a GUI passa a consultar RWD, MMOD, MVEL e MXRPM dos dois canais. MMOD3 seleciona automaticamente P no canal parado; essa seleção não grava configuração nem comanda movimento. G diferente de zero é recusado pela interface quando MMOD3 foi lido.

Em P, arrastar a barra mostra o próximo destino; soltar aplica e mantém esse destino absoluto. Alterações discretas por teclado ou acessibilidade também aplicam. Aplicar continua disponível, inclusive para confirmar novamente o zero após Parar. Enquanto a barra está sendo arrastada, o destino confirmado anteriormente permanece. Parar usa MS sem retornar à origem e preserva a habilitação manual. A abertura da janela continua sem autorizar movimento.
