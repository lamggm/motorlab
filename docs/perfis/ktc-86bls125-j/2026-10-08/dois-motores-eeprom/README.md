# Dois KTC-86BLS125-J: configuração salva na EEPROM

Em 08/10/2026, o usuário autorizou substituir o perfil antigo na EEPROM e duplicar a configuração para o Motor 2, do mesmo modelo, ainda por conectar. Unidade SBLG2360T, firmware v3.0f de 15/07/2026.

Foram pareados e conferidos **74 campos** do motor/encoder: 25 alterações na RAM do canal 2, todas com ACK e releitura individual. A comparação integral posterior encontrou exatamente essas 25 diferenças em 636 registros conhecidos; o Motor 1 e os demais registros comparáveis permaneceram iguais. `%EESAV` recebeu `+`, seguido de releitura de todo o catálogo. **Não foi realizado ciclo de alimentação**; o ACK confirma a operação aceita, mas a recuperação após reinício ainda não foi ensaiada.

Ambos os canais têm controle de velocidade por encoder, comutação trapezoidal por Hall, 3000 RPM, ALIM/NOMA de 19 A, 660 W, quatro pares de polos, aceleração de 3000 RPM/s, desaceleração de 2000 RPM/s e PI inicial Kp=0,03/Ki=0,01. Ganhos de posição e corrente também foram pareados, mas continuam sem qualificação para posição/FOC neste motor. As tabelas de calibração física Hall/SinCos foram preservadas em seus respectivos canais, não copiadas como se fossem válidas para o conjunto novo.

O encoder 1 permanece associado ao Motor 1 (`EMOD 1=18`). O encoder 2 foi preparado para feedback do Motor 2 (`EMOD 2=34`, uso 2 + associação 32), com `EPPR 2=1000`. Ligue o segundo AS5047P a A2/B2; não usar o encoder 1 como feedback dos dois motores. Sentido, resolução real e resposta do segundo conjunto ainda precisam ser verificados após a ligação.

## Estado final e falha do canal ainda desconectado

Não foi enviado comando de movimento nesta duplicação. Ambos os motores ficaram parados, com EX mantido. `?FF=48` contém EStop (16) e Motor/Sensor (32). Uma consulta serial somente de leitura confirmou Hall 1=2, válido, e **Hall 2=7, inválido para os sensores de 120°** (`PSA 1/2=120`). O Motor 2 ainda desconectado é compatível com essa leitura inválida. A falha pode impedir o acionamento do conjunto; não houve desativação da detecção estrita (`SED 1/2=2`) nem liberação EX/MG para contorná-la. Conectar fases, alimentação/sinais Hall e encoder corretamente antes de validar e liberar. Mudanças de fiação devem ser feitas com a potência desligada.

CLI e porta serial ficaram desconectadas ao término. A EEPROM agora corresponde ao motor novo nos dois canais; o [perfil antigo arquivado](../../../ktc-86bls98-j/2026-10-08/README.md) continua disponível no Git.

## Evidências

- [Perfil salvo e relido](perfil-final-eeprom.json)
- [Plano, índices e campos não copiados](plano.json)
- [Aplicação na RAM](aplicacao-ram.json)
- [ACK da gravação EEPROM](gravacao-eeprom.json)
- [Releitura completa](releitura-pos-gravacao.json)
- [Comparação final](comparacao-final.json)
- [Estado parado](estado-parado.json)
- [Estados Hall e falhas](diagnostico-hall-pos-gravacao.json)
- [SHA-256 e alcance de validação](manifesto.json)
