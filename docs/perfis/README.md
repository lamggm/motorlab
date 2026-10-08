# Perfis de motores da bancada

Cada perfil identifica o motor, a unidade Roboteq, o firmware, a data da leitura e o alcance da validação. O JSON preserva valores brutos e suas escalas para importação como rascunho no MotorLab. Importar não escreve no driver; aplicar na RAM e salvar EEPROM são operações separadas.

| Motor | Perfil preservado | Situação |
|---|---|---|
| Kalatec KTC-86BLS98-J/X046, 400 W | [08/10/2026](ktc-86bls98-j/2026-10-08/README.md) | 636 parâmetros conhecidos, posição selecionada, limites 3000 RPM / 11,5 A e confirmação de gravação EEPROM |
| Kalatec KTC-86BLS125-J, 660 W | [Primeiro teste, 08/10/2026](ktc-86bls125-j/2026-10-08/README.md) | 100 e 500 RPM por encoder, comutação trapezoidal Hall, 3000 RPM / 19 A; EEPROM nos dois canais, sintonia inicial; Motor 2 ainda desconectado |

Os perfis incluem parâmetros globais e do Motor 2, além do Motor 1. A calibração Hall, os ganhos e a configuração do encoder pertencem ao conjunto ensaiado. Não aplicar o perfil do KTC-86BLS98-J ao KTC-86BLS125-J.
