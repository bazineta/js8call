// -*- Mode: C++ -*-
/*
 * Class to handle the formatted string as returned from the fortran decoder
 *
 * VK3ACF August 2013
 */


#ifndef DECODEDTEXT_H
#define DECODEDTEXT_H

#include "JS8.hpp"
#include <QString>
#include <QStringList>

class DecodedText
{
public:

  // Constructors

  explicit DecodedText(JS8::Event::Decoded const &);
  explicit DecodedText(QString const & frame,
                       int             bits,
                       int             submode);

  // Inline accessors

  int         bits()              const { return bits_;                  }
  QString     compoundCall()      const { return compound_;              }
  QStringList directedMessage()   const { return directed_;              }
  float       dt()                const { return dt_;                    }
  QString     extra()             const { return extra_;                 }
  QString     frame()             const { return frame_;                 }
  quint8      frameType()         const { return frameType_;             }
  int         frequencyOffset()   const { return frequencyOffset_;       }
  bool        isAlt()             const { return isAlt_;                 }
  bool        isCompound()        const { return !compound_.isEmpty();   }
  bool        isDirectedMessage() const { return directed_.length() > 2; }
  bool        isHeartbeat()       const { return isHeartbeat_;           }
  bool        isLowConfidence ()  const { return isLowConfidence_;       }
  QString     message()           const { return message_;               }
  int         snr()               const { return snr_;                   }
  int         submode()           const { return submode_;               }
  int         time()              const { return time_;                  }

  // Accessors

  QStringList messageWords () const;
  QString     string()        const;

private:

  // Unpacking strategies entry point and strategies.

  bool tryUnpack();
  bool tryUnpackCompound();
  bool tryUnpackData();
  bool tryUnpackDirected();
  bool tryUnpackFastData();
  bool tryUnpackHeartbeat();

  // Specific unpacking strategies, attempted in order until one works
  // or all have failed.

  std::array<bool (DecodedText::*)(), 5> unpackStrategies =
  {
    &DecodedText::tryUnpackFastData,
    &DecodedText::tryUnpackData,
    &DecodedText::tryUnpackHeartbeat,
    &DecodedText::tryUnpackCompound,
    &DecodedText::tryUnpackDirected
  };

  // Data members ** ORDER DEPENDENCY **

  quint8      frameType_;
  QString     frame_;
  bool        isAlt_;
  bool        isHeartbeat_;
  bool        isLowConfidence_;
  QString     compound_;
  QStringList directed_;
  QString     extra_;
  QString     message_;
  int         bits_;
  int         submode_;
  int         time_;
  int         frequencyOffset_;
  int         snr_;
  float       dt_;
};

#endif // DECODEDTEXT_H
