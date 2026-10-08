# Meta de baixa velocidade em bancada

Decisão de 07/10/2026: adotar **20 RPM no eixo do motor** como referência experimental de baixa rotação contínua para a sintonia.

## Objetivo do ensaio

Avaliar partida confiável e giro contínuo a 20 RPM no eixo do motor, observando a regularidade do movimento, a evolução do contador do encoder e a leitura instantânea de RPM durante várias voltas. Uma média correta isoladamente não comprova uniformidade. As tolerâncias quantitativas de velocidade ainda precisam ser definidas para a avaliação.

## Alcance da decisão e validação pendente

Os **20 RPM são uma meta experimental de controle suave**, não um limite mínimo imposto ao driver nem uma velocidade mínima garantida pelo fabricante. Zero continua sendo um comando válido de parada normal. Este registro não altera parâmetros, RAM, EEPROM ou comandos de movimento.

Antes dessa rodada, os ensaios haviam validado pontos a partir de 100 RPM na bancada sem carga; 20 RPM ainda não tinha sido validado. A atualização abaixo registra os ensaios posteriores. Comportamento sob carga, atrito e folga mecânica exigem avaliação própria.

Referência dos ensaios: [VALIDACAO.md](../VALIDACAO.md).

## Atualização após a sintonia

Em 07/10/2026, após essa decisão, dois ensaios a 20 RPM foram concluídos com médias de 20,06 e 19,88 RPM. O operador considerou aceitável o giro observado na bancada, com oscilação residual. Foram também ensaiados 100, 500, 1500 e 3000 RPM. Consulte [parâmetros, resultados e limitações da sintonia](SINTONIA-20RPM.md). A configuração ficou apenas na RAM; não foi gravada na EEPROM nessa rodada.
