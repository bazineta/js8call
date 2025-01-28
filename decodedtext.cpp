#include "decodedtext.h"

#include <QStringList>
#include <QRegularExpression>
#include <QDebug>

#include <varicode.h>

DecodedText::DecodedText (QString const& the_string)
  : frameType_(Varicode::FrameUnknown)
  , isHeartbeat_(false)
  , isAlt_(false)
  , string_ {the_string.left (the_string.indexOf (QChar::Nbsp))} // discard appended info
  , padding_ {string_.indexOf (" ") > 4 ? 2 : 0} // allow for seconds
  , message_ {string_.mid (column_qsoText + padding_).trimmed ()}
  , bits_{0}
  , submode_{ string_.mid(column_mode + padding_, 3).trimmed().at(0).cell() - 'A' }
  , frame_ { string_.mid (column_qsoText + padding_, 12).trimmed () }
{
    if(message_.length() >= 1) {
        message_ = message_.left (21).remove (QRegularExpression {"[<>]"});
        int i1 = message_.indexOf ('\r');
        if (i1 > 0) {
            message_ = message_.left (i1 - 1);
        }
    }

    bits_ = bits();

    tryUnpack();
}

DecodedText::DecodedText (QString const& js8callmessage, int bits, int submode):
    frameType_(Varicode::FrameUnknown),
    isHeartbeat_(false),
    isAlt_(false),
    message_(js8callmessage),
    bits_(bits),
    submode_(submode),
    frame_(js8callmessage)
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

bool DecodedText::tryUnpackHeartbeat(){
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

bool DecodedText::tryUnpackCompound(){
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

bool DecodedText::tryUnpackDirected(){
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

bool DecodedText::tryUnpackData(){
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

bool DecodedText::tryUnpackFastData(){
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

QStringList DecodedText::messageWords () const
{
  // simple word split for free text messages
  auto words = message_.split (' ', Qt::SkipEmptyParts);
  // add whole message as item 0 to mimic RE capture list
  words.prepend (message_);
  return words;
}

bool DecodedText::isLowConfidence () const
{
  return QChar {'?'} == string_.mid (padding_ + column_qsoText + 21, 1);
}

int DecodedText::frequencyOffset() const
{
    return string_.mid(column_freq + padding_,4).toInt();
}

int DecodedText::snr() const
{
  int i1=string_.indexOf(" ")+1;
  return string_.mid(i1,3).toInt();
}

float DecodedText::dt() const
{
  return string_.mid(column_dt + padding_,5).toFloat();
}
