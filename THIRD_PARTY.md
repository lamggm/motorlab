# Materiais e dependências de terceiros

A licença MIT em LICENSE se aplica ao código original do MotorLab Linux e à documentação produzida para este aplicativo. Não altera os direitos sobre Qt, dfu-util, firmware, manuais ou marcas Roboteq/RoboRun+.

- Qt 6: dependência dinâmica instalada no sistema, com licenças próprias. Não incluída no pacote.
- dfu-util: ferramenta externa instalada pelo usuário, com licença própria. Não incluída no pacote.
- `firmware/*.dfu` e os respectivos readmes: imagens e notas originais da Roboteq, preservadas sem alteração, baixadas dos URLs oficiais em `data/firmware.json` em 06/10/2026. A inclusão não implica autoria, garantia ou licença MIT para esses arquivos.
- `docs/RoboRun-Plus-User-Manual-v3.2.pdf`: manual original da Roboteq, obtido do site oficial em 06/10/2026. SHA-256: `9726a30b624769b0f160fa9803f9f43f41665ddfc758b5bc7803783366b7e82c`.
- `data/catalog.json`: seleção de comandos, limites e escalas extraídos do Controllers User Manual v3.0c e revisados para os modelos suportados. O script de extração está em `tools/generate_catalog.py`; reproduzir a geração exige o texto do manual via pdftotext -layout. As referências e páginas constam do catálogo e README.

A versão 0.4 inclui componentes derivados do pacote fornecido pelo usuário, separados do código MIT:

- `compiler/vendor/`: núcleo de compilação e interpretação MicroBasic, recuperado de `MicroBasic.dll`, e tabelas de constantes de `Controller.dll` do RoboRun+ 3.3. Código derivado da Roboteq; **não coberto pela licença MIT do MotorLab**. Alterações funcionais do adaptador ficam em `compiler/Program.cs`. Não foram incluídos editores WinForms, rotinas de licença nem drivers Windows.
- `compiler/Irony.dll`: parser gerenciado original do pacote, usado como dependência do núcleo. Atribuição Irony/.NET Language Implementation Kit; mantém os direitos/licença do componente original. Nenhuma licença de redistribuição adicional é concedida pelo MotorLab.
- `data/original-trees.json` e `tests/fixtures/`: metadados, perfil e scripts originais/derivados do pacote Roboteq, preservados para compatibilidade e testes locais. Não cobertos pela licença MIT.
- `libexec/compiler/`: helper Linux x86_64 com runtime .NET 8.0.31 autocontido. As licenças e avisos do runtime estão em `LICENSE.txt` e `THIRD-PARTY-NOTICES.txt` nessa pasta. Não exige Wine nem instalação global de .NET.

O ZIP de origem e os arquivos usados estão identificados por SHA-256 em `docs/roborun-3.3-provenance.json`. A origem foi uma entrega local para o usuário que forneceu o pacote. Em 07/10/2026, os fontes e insumos de compatibilidade foram preparados para versionamento no repositório privado `lamggm/motorlab`, por solicitação do usuário. Isso não altera as licenças ou os direitos dos componentes de terceiros. Binários gerados e o runtime autocontido não são versionados. Roboteq e RoboRun+ são marcas de seus titulares.
