#ifndef MAIDENHEAD_HPP__
#define MAIDENHEAD_HPP__

#include <QString>
#include <QStringView>
#include <QValidator>

namespace Maidenhead
{
  // Given a string view, return the index at which the view fails to
  // contain a valid id, or QStringView::size() if the view is valid.
  //
  // Note carefully the following:
  //
  //    1. A view that's incomplete, but still valid up to the point
  //       of being incomplete, is valid.
  //    2. An odd-length view is, therefore, valid.
  //    3. A zero-length view is also valid.
  //
  // There is therfore more validation required above this point; the
  // only assertion we make on completely valid input is that it's ok
  // so far, but we're not asserting that it's complete.
  //
  // Validation is case-insensitive. While the standard defines pairs
  // containing alphabetic characters as being upper case, the older
  // QRA standard used lower case, and various software packages do
  // either or both, so we're being liberal in what we accept.
  
  constexpr auto
  invalidIndex(QStringView const view) noexcept
  {
    // Given a numeric Unicode value, return the upper case version if
    // it lies within the range of lower case alphabetic characters.

    auto const normalize = [](auto const u) noexcept
    {
      return (u >= u'a' && u <= u'z')
           ?  u - (u'a' - u'A')
           :  u;
    };

    // Standard Maidenhead identifiers must be exactly 4, 6 or 8
    // characters. Indices and valid values for the pairs are:
    //
    //   1. Field:     [0, 1]: [A, R]
    //   2. Square:    [2, 3]: [0, 9]
    //   3. Subsquare: [4, 5]: [A, X]
    //   4. Extended:  [6, 7]: [0, 9]
    //
    // Nonstandard extensions exist in domains such as APRS, which
    // add up to an additional two pairs:
    // 
    //   5. Ultra Extended: [ 8,  9]: [A, X]
    //   6. Hyper Extended: [10, 11]: [0, 9]

    auto const size = view.size();

    for (qsizetype i = 0; i < size; ++i)
    {
      auto const u = normalize(view[i].unicode());

      switch (i)
      {
        case  0: case  1: if (u >= u'A' && u <= u'R') continue; break;
        case  2: case  3:
        case  6: case  7:
        case 10: case 11: if (u >= u'0' && u <= u'9') continue; break;
        case  4: case  5: 
        case  8: case  9: if (u >= u'A' && u <= u'X') continue; break;
      }
      return i;
    }

    return size;
  }

  // Given a string view, return true if it has a length compatible with
  // containment of the range of pairs requested, and the data within it
  // is valid over the complete span, false otherwise.

  template <qsizetype Min = 2,
            qsizetype Max = 6>
  constexpr auto
  valid(QStringView const view) noexcept
  {
    static_assert(Min >=   1);
    static_assert(Max >=   1);
    static_assert(Min <=   6);
    static_assert(Max <=   6);
    static_assert(Min <= Max);

    if (auto const size = view.size();
                 !(size  & 1)       &&
                  (size >= 2 * Min) &&
                  (size <= 2 * Max))
    {
      return invalidIndex(view) == size;
    }

    return false;
  }

    // Valid test cases.

  static_assert(valid(u"AA00"));
  static_assert(valid(u"AA00AA"));
  static_assert(valid(u"AA00AA00"));
  static_assert(valid(u"BP51AD95RF"));
  static_assert(valid(u"BP51AD95RF00"));
  static_assert(valid(u"aa00"));
  static_assert(valid(u"AA00aa"));
  static_assert(valid(u"RR00XX"));

  // Invalid test cases.

  static_assert(!valid(u""));
  static_assert(!valid(u"A"));
  static_assert(!valid(u"0"));
  static_assert(!valid(u"AA00 "));
  static_assert(!valid(u"AA00 "));
  static_assert(!valid(u"AA00 "));
  static_assert(!valid(u" AA00"));
  static_assert(!valid(u" AA00"));
  static_assert(!valid(u"00"));
  static_assert(!valid(u"aa00a"));
  static_assert(!valid(u"AA00ZZA"));
  static_assert(!valid(u"!@#$%^"));
  static_assert(!valid(u"123456"));
  static_assert(!valid(u"AA00ZZ"));
  static_assert(!valid(u"ss00XX"));
  static_assert(!valid(u"rr00yy"));
  static_assert(!valid(u"AAA1aa"));
  static_assert(!valid(u"BP51AD95RF00A"));
  static_assert(!valid(u"BP51AD95RF0000"));

  // Template specifying a QValidator, where the minimum and maximum
  // number of acceptable pairs must be specified.
  //
  // In order for an input to be acceptable, at least the minimum number
  // of pairs must be provided, no more than the maximum can be provided,
  // and all pairs must be valid.

  template <qsizetype Min,
            qsizetype Max>
  class Validator final : public QValidator
  {
    static_assert(Min >=   1);
    static_assert(Max >=   1);
    static_assert(Min <=   6);
    static_assert(Max <=   6);
    static_assert(Min <= Max);

    using QValidator::QValidator;

    State
    validate(QString & input,
             int     & pos) const override
    {
      // Ensure the input is upper case and get the size.

      input           = input.toUpper();
      auto const size = input.size();

      // If nothing's been entered, we need more from them; if over
      // the maximum, less.

      if (size == 0)      return Intermediate;
      if (size > Max * 2) return Invalid;

      // If anything up to the cursor is invalid, then we're invalid.
      // Anything after the cursor, we're willing to be hopeful about.

      if (auto const index  = invalidIndex(input);
                     index != size)
      {
        return index < pos
             ? Invalid
             : Intermediate;
      }

      // Entire input was valid. If the count is odd, or we haven't yet
      // hit the minimum, we need more from them, otherwise, we're good.

      return ((size & 1) || (size < Min * 2))
           ? Intermediate
           : Acceptable;
    }
  };

  // Convenience definitions:
  //
  //   Standard: User must specify field and square, and can optionally
  //             specify subsquare. Ideal for QSO logging.
  //
  //   Extended: User must specify field and square, and can optionally
  //             specify subsquare, extended, ultra extended, and hyper
  //             extended. Ideal for station grid entry.

  using StandardValidator = Validator<2,3>;
  using ExtendedValidator = Validator<2,6>;
}

#endif
