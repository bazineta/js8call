#ifndef MAIDENHEAD_HPP__
#define MAIDENHEAD_HPP__

#include <QValidator>

namespace Maidenhead
{
  template <qsizetype Min,
            qsizetype Max>
  class Validator final : public QValidator
  {
    static_assert(Min >   0);
    static_assert(Max >   0);
    static_assert(Min <=  6);
    static_assert(Max <=  6);
    static_assert(Min < Max);

    using QValidator::QValidator;

    State validate(QString & input,
                   int     &) const override
    {
      auto const size = input.size();

      // If nothing's been entered, we need more from them; if over the
      // the maximum, less.

      if (size ==      0) return Intermediate;
      if (size > Max * 2) return Invalid;

      // Standard Maidenhead grid squares must be exactly 4, 6 or 8
      // characters. Valid values for the pairs are:
      //
      //   1: Field     [0, 1]: A-R, inclusive
      //   2: Square    [2, 3]: 0-9, inclusive
      //   3: Subsquare [4, 5]: A-X, inclusive
      //   4: Extended  [6, 7]: 0-9, inclusive
      //
      // Nonstandard extensions exist in domains such as APRS, which
      // add up to an additional two pairs:
      // 
      //   5: Ultra Extended: [ 8,  9]: A-X, inclusive
      //   6: Hyper Extended: [10, 11]: 0-9, inclusive

      input = input.toUpper();

      for (qsizetype i = 0; i < size; ++i)
      {
        auto const u = input[i].unicode();

        switch (i)
        {
          case  0: case  1: if (!(u >= u'A' && u <= u'R')) return Invalid; break;
          case  2: case  3:
          case  6: case  7:
          case 10: case 11: if (!(u >= u'0' && u <= u'9')) return Invalid; break;
          case  4: case  5: 
          case  8: case  9: if (!(u >= u'A' && u <= u'X')) return Invalid; break;
        }
      }

      // If the count is odd, or we haven't yet hit the minimum, we need
      // more from them, otherwise, we're good.

      return ((size & 1) || (size < Min * 2)) ? Intermediate : Acceptable;
    }
  };
}

#endif
