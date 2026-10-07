#include "hexfile.h"
#include "protocol.h"
#include <QMap>
HexImage HexFile::parse(const QByteArray &text) {
  HexImage image;
  QMap<int, char> memory;
  quint32 base = 0;
  bool eof = false;
  auto bad = [&](const QString &e) {
    image.error = e;
    image.records.clear();
    return image;
  };
  if (text.size() > 256 * 1024 || text.isEmpty())
    return bad("Arquivo HEX vazio ou maior que 256 KiB.");
  for (const auto &raw : text.split('\n')) {
    const auto line = QString::fromLatin1(raw.trimmed());
    if (line.isEmpty())
      continue;
    if (eof)
      return bad("Há dados depois do fim do HEX.");
    const auto c = Protocol::command(line);
    if (!c.valid || !c.hex)
      return bad("Registro HEX inválido ou checksum incorreto.");
    const auto b = QByteArray::fromHex(line.mid(1).toLatin1());
    const int len = quint8(b[0]);
    const int type = quint8(b[3]);
    const quint32 address = (quint8(b[1]) << 8) | quint8(b[2]);
    if (type == 0) {
      for (int i = 0; i < len; ++i) {
        const quint64 at = quint64(base) + address + i;
        if (at >= 32768)
          return bad("Endereço fora da memória de scripts (32 KiB). Não envie "
                     "firmware neste carregador.");
        if (memory.contains(int(at)))
          return bad("Registros HEX sobrepostos.");
        memory[int(at)] = b[4 + i];
      }
    } else if (type == 1) {
      if (len != 0 || address != 0)
        return bad("Fim de arquivo inválido.");
      eof = true;
    } else if (type == 2 || type == 4) {
      if (len != 2 || address != 0)
        return bad("Endereço estendido inválido.");
      base = quint32((quint8(b[4]) << 8) | quint8(b[5]))
             << (type == 2 ? 4 : 16);
    } else
      return bad("Tipo de registro não aceito para bytecode MicroBasic.");
    image.records.append(line.toUpper());
  }
  if (!eof || memory.isEmpty())
    return bad("HEX incompleto, sem dados ou sem registro final.");
  if (memory.firstKey() != 0)
    return bad("Bytecode deve começar no endereço zero.");
  for (int i = 0; i <= memory.lastKey(); ++i) {
    if (!memory.contains(i))
      return bad("HEX com lacuna de endereços.");
    image.bytes.append(memory[i]);
  }
  return image;
}
