#include "decodedtext.h"
#include <QStringList>
#include <QRegularExpression>
#include <QDebug>
#include <varicode.h>

namespace
{
  constexpr auto QUALITY_THRESHOLD = 0.17f;

  QChar
  submodeChar(int const submode)
  {
    switch (submode)
    {
      case 0:  return 'A';
      case 1:  return 'B';
      case 2:  return 'C';
      case 4:  return 'E';
      case 8:  return 'I';
      default: return '~';
    }
  }
}

DecodedText::DecodedText(JS8::Event::Decoded const & decoded)
: frameType_      (Varicode::FrameUnknown)
, frame_          (QString::fromStdString(decoded.data))
, isAlt_          (false)
, isHeartbeat_    (false)
, isLowConfidence_(decoded.quality < QUALITY_THRESHOLD)
, message_        (frame_)
, bits_           (decoded.type)
, submode_        (decoded.mode)
, time_           (decoded.utc)
, frequencyOffset_(decoded.frequency)
, snr_            (decoded.snr)
, dt_             (decoded.xdt)
{
  tryUnpack();
}

DecodedText::DecodedText(QString const & frame,
                        int      const   bits,
                        int      const   submode)
: frameType_      (Varicode::FrameUnknown)
, frame_          (frame)
, isAlt_          (false)
, isHeartbeat_    (false)
, isLowConfidence_(false)
, message_        (frame_)
, bits_           (bits)
, submode_        (submode)
, time_           (0)
, frequencyOffset_(0)
, snr_            (0)
, dt_             (0.0f)
{
    tryUnpack();
}

bool
DecodedText::tryUnpack()
{
  for (auto unpack : unpackStrategies)
  {
    if ((this->*unpack)()) return true;
  }

  return false;
}

bool
DecodedText::tryUnpackHeartbeat()
{
    QString m = message().trimmed();

    // directed calls will always be 12+ chars and contain no spaces.
    if(m.length() < 12 || m.contains(' ')){
      return false;
    }

    if((bits_ & Varicode::JS8CallData) == Varicode::JS8CallData){
            return false;
    }

    bool isAlt = false;
    quint8 type = Varicode::FrameUnknown;
    quint8 bits3 = 0;
    QStringList parts = Varicode::unpackHeartbeatMessage(m, &type, &isAlt, &bits3);

    if(parts.isEmpty() || parts.length() < 2){
        return false;
    }

    // Heartbeat Alt Type
    // ---------------
    // 1      0   HB
    // 1      1   CQ
    isHeartbeat_ = true;
    isAlt_ = isAlt;
    extra_ = parts.length() < 3 ? "" : parts.at(2);

    QStringList cmp;
    if(!parts.at(0).isEmpty()){
        cmp.append(parts.at(0));
    }
    if(!parts.at(1).isEmpty()){
        cmp.append(parts.at(1));
    }
    compound_ = cmp.join("/");

    if(isAlt){
        auto sbits3 = Varicode::cqString(bits3);
        message_ = QString("%1: @ALLCALL %2 %3 ").arg(compound_).arg(sbits3).arg(extra_);
        frameType_ = type;
    } else {
        auto sbits3 = Varicode::hbString(bits3);
        if(sbits3 == "HB"){
            message_ = QString("%1: @HB HEARTBEAT %2 ").arg(compound_).arg(extra_);
            frameType_ = type;
        } else {
            message_ = QString("%1: @HB %2 %3 ").arg(compound_).arg(sbits3).arg(extra_);
            frameType_ = type;
        }
    }

    return true;
}

bool
DecodedText::tryUnpackCompound()
{
    auto m = message().trimmed();
    // directed calls will always be 12+ chars and contain no spaces.
    if(m.length() < 12 || m.contains(' ')){
        return false;
    }

    quint8 type = Varicode::FrameUnknown;
    quint8 bits3 = 0;
    auto parts = Varicode::unpackCompoundMessage(m, &type, &bits3);
    if(parts.isEmpty() || parts.length() < 2){
        return false;
    }

    if((bits_ & Varicode::JS8CallData) == Varicode::JS8CallData){
        return false;
    }

    QStringList cmp;
    if(!parts.at(0).isEmpty()){
        cmp.append(parts.at(0));
    }
    if(!parts.at(1).isEmpty()){
        cmp.append(parts.at(1));
    }
    compound_ = cmp.join("/");
    extra_ = parts.length() < 3 ? "" : parts.mid(2).join(" ");

    if(type == Varicode::FrameCompound){
        message_ = QString("%1: ").arg(compound_);
    } else if(type == Varicode::FrameCompoundDirected){
        message_ = QString("%1%2 ").arg(compound_).arg(extra_);
        directed_ = QStringList{ "<....>", compound_ } + parts.mid(2);
    }

    frameType_ = type;
    return true;
}

bool
DecodedText::tryUnpackDirected()
{
    QString m = message().trimmed();

    // directed calls will always be 12+ chars and contain no spaces.
    if(m.length() < 12 || m.contains(' ')){
      return false;
    }

    if((bits_ & Varicode::JS8CallData) == Varicode::JS8CallData){
        return false;
    }

    quint8 type = Varicode::FrameUnknown;
    QStringList parts = Varicode::unpackDirectedMessage(m, &type);

    if(parts.isEmpty()){
      return false;
    }

    if(parts.length() == 3){
      // replace it with the correct unpacked (directed)
      message_ = QString("%1: %2%3 ").arg(parts.at(0), parts.at(1), parts.at(2));
    } else if(parts.length() == 4){
      // replace it with the correct unpacked (directed numeric)
      message_ = QString("%1: %2%3 %4 ").arg(parts.at(0), parts.at(1), parts.at(2), parts.at(3));
    } else {
      // replace it with the correct unpacked (freetext)
      message_ = QString(parts.join(""));
    }

    directed_ = parts;
    frameType_ = type;
    return true;
}

bool
DecodedText::tryUnpackData()
{
    QString m = message().trimmed();

    // data frames calls will always be 12+ chars and contain no spaces.
    if(m.length() < 12 || m.contains(' ')){
        return false;
    }

    if((bits_ & Varicode::JS8CallData) == Varicode::JS8CallData){
        return false;
    }

    QString data = Varicode::unpackDataMessage(m);

    if(data.isEmpty()){
      return false;
    }

    message_ = data;
    frameType_ = Varicode::FrameData;
    return true;
}

bool
DecodedText::tryUnpackFastData()
{
    QString m = message().trimmed();

    // data frames calls will always be 12+ chars and contain no spaces.
    if(m.length() < 12 || m.contains(' ')){
        return false;
    }

    if((bits_ & Varicode::JS8CallData) != Varicode::JS8CallData){
        return false;
    }

    QString data = Varicode::unpackFastDataMessage(m);

    if(data.isEmpty()){
      return false;
    }

    message_ = data;
    frameType_ = Varicode::FrameData;
    return true;
}

QStringList
DecodedText::messageWords() const
{
  // simple word split for free text messages
  auto words = message_.split (' ', Qt::SkipEmptyParts);
  // add whole message as item 0 to mimic RE capture list
  words.prepend (message_);
  return words;
}

// Format as a string suitable for appending to ALL.TXT. Original
// code has no space between time and SNR; matching that here.

QString
DecodedText::string() const
{
  return QStringLiteral("%1:%2:%3%4 %5 %6 %7  %8         %9   ")
    .arg( time_ / 10000,      2, 10, QChar('0'))   // Extract hours
    .arg((time_ / 100) % 100, 2, 10, QChar('0'))   // Extract minutes
    .arg( time_        % 100, 2, 10, QChar('0'))   // Extract seconds
    .arg(snr_,                3, 10, QChar(' '))   // Right-aligned integer with 3 characters, padded with spaces
    .arg(dt_,                 4, 'f', 1)           // Right-aligned float with 1 decimal point
    .arg(frequencyOffset_,    4, 10, QChar(' '))   // Right-aligned float with no decimal points
    .arg(submodeChar(submode_))                    // Single character
    .arg(frame_)                                   // Fixed string, 12 characters
    .arg(bits_);                                   // Single 3-bit integer
}
