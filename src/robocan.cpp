#include "robocan.h"
QString microbasicRoboCan(const HexImage &image) {
  if (!image.valid() || image.bytes.isEmpty() || image.bytes.size() % 16)
    return {};
  QString text = "; MotorLab Linux - MicroBasic RoboCAN transcript\n; SS = "
                 "source node; TT = target node; XX = unchecked byte; X = any "
                 "length\n; Offline export. Never executed by MotorLab.\n; "
                 "Emergency stop remains engaged at completion.\n\n";
  text += "; Target heartbeat\n<-- 000   2   TT XX\n; Source heartbeat\n--> "
          "000   2   SS 00\n; Emergency stop\n--> TT6   4   SS 0E 14 00\n\n";
  text += "; Download request / acknowledgement\n--> TT1   8   SS 02 14 FF CB "
          "10 2C 13\n<-- SS9   X   TT 02 XX B0 XX XX XX XX\n\n";
  const auto hex = [](int value) {
    return QString("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
  };
  for (int i = 0; i < image.bytes.size(); i += 4) {
    text += "--> TT2   8   SS 0F " + hex(i & 255) + " " + hex((i >> 8) & 255);
    for (int j = 0; j < 4; ++j)
      text += " " + hex(quint8(image.bytes[i + j]));
    text += "\n";
    if (i % 16 == 12)
      text += "<-- SS9   X   TT 02 XX C0 XX XX XX XX\n\n";
  }
  text += "; Commit script\n--> TT2   4   SS F0 2C 00\n<-- SS9   X   TT 02 XX "
          "D0 XX XX XX XX\n; No release / no script run.\n";
  return text;
}
