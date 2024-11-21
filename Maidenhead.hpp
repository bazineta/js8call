#ifndef MAIDENHEAD_HPP__
#define MAIDENHEAD_HPP__

#include <QValidator>

namespace Maidenhead
{
  // Given a string view and a span within it, return the index of the
  // point at which the span fails to contain a valid grid, or size if
  // the span is valid. Note carefully the following:
  //
  //    1. A span that's incomplete, but still valid up to the point
  //       of being incomplete, is valid.
  //    2. An odd-length span is, therefore, valid.
  //    3. A zero-length span is also valid.
  //
  // There is therfore more validation required above this point; the
  // only assertion we make on completely valid input is that it's ok
  // so far, but we're not asserting that it's complete.
  
  constexpr auto
  invalidIndex(QStringView const view,
               qsizetype   const base,
               qsizetype   const size) noexcept
  {
    // Given a numeric Unicode value, return the upper case version if
    // it lies within the range of lower case alphabetic characters.

    auto const normalize = [](auto const u)
    {
      return (u >= u'a' && u <= u'z')
           ?  u - (u'a' - u'A')
           :  u;
    };

    // Standard Maidenhead grid squares must be exactly 4, 6 or 8
    // characters. Valid values for the pairs are:
    //
    //   1: Field     [0, 1]: A-R, inclusive, required
    //   2: Square    [2, 3]: 0-9, inclusive, required
    //   3: Subsquare [4, 5]: A-X, inclusive, optional
    //   4: Extended  [6, 7]: 0-9, inclusive, optional
    //
    // Nonstandard extensions exist in domains such as APRS, which
    // add up to an additional two pairs:
    // 
    //   5: Ultra Extended: [ 8,  9]: A-X, inclusive, optional
    //   6: Hyper Extended: [10, 11]: 0-9, inclusive, optional

    for (qsizetype i = 0; i < size; ++i)
    {
      auto const u = normalize(view[base + i].unicode());

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

  // Given a string view, return true if it contains a valid grid of at
  // least the minimum number of pairs, and no more than the maximum number
  // of pairs.

  template <qsizetype Min = 2,
            qsizetype Max = 6>
  constexpr auto
  isValidGrid(QStringView const view) noexcept
  {
    static_assert(Min >=   1);
    static_assert(Max >=   1);
    static_assert(Min <=   6);
    static_assert(Max <=   6);
    static_assert(Min <= Max);

    // For a span to be valid, it must have an even number of bytes, and
    // be able to contain at least the minimum number of pairs requested
    // and no more than the maximum number requested.

    if (auto const size = view.size();
                 !(size  & 1)       &&
                  (size >= 2 * Min) &&
                  (size <= 2 * Max))
    {
      return invalidIndex(view, 0, size) == size;
    }

    return false;
  }

    // Valid test cases.

  static_assert(isValidGrid(u"AA00"));
  static_assert(isValidGrid(u"AA00AA"));
  static_assert(isValidGrid(u"AA00AA00"));
  static_assert(isValidGrid(u"BP51AD95RF"));
  static_assert(isValidGrid(u"BP51AD95RF00"));
  static_assert(isValidGrid(u"aa00"));
  static_assert(isValidGrid(u"AA00aa"));
  static_assert(isValidGrid(u"RR00XX"));

  // Invalid test cases.

  static_assert(!isValidGrid(u""));
  static_assert(!isValidGrid(u"A"));
  static_assert(!isValidGrid(u"A "));
  static_assert(!isValidGrid(u" A"));
  static_assert(!isValidGrid(u" 00"));
  static_assert(!isValidGrid(u"aa00a"));
  static_assert(!isValidGrid(u"AA00ZZA"));
  static_assert(!isValidGrid(u"!@#$%^"));
  static_assert(!isValidGrid(u"123456"));
  static_assert(!isValidGrid(u"AA00ZZ"));
  static_assert(!isValidGrid(u"ss00XX"));
  static_assert(!isValidGrid(u"rr00yy"));
  static_assert(!isValidGrid(u"AAA1aa"));
  static_assert(!isValidGrid(u"BP51AD95RF00A"));
  static_assert(!isValidGrid(u"BP51AD95RF0000"));

  // Given a string view, return true if a trimmed version of the view
  // contains a valid grid of at least the minimum number of pairs, and
  // no more than the maximum number of pairs.

  template <qsizetype Min = 2,
            qsizetype Max = 6>
  constexpr auto
  containsValidGrid(QStringView const view) noexcept
  {
    static_assert(Min >=   1);
    static_assert(Max >=   1);
    static_assert(Min <=   6);
    static_assert(Max <=   6);
    static_assert(Min <= Max);

    // Any amount of whitespace surrounding the grid square is ok; find
    // the indices of the first and last non-whitespace characters.

    qsizetype start = 0;
    qsizetype end   = view.size();

    while (start < end && view[start].isSpace()) ++start;
    while (end > start && view[end - 1].isSpace()) --end;

    // For a span to be valid, it must have an even number of bytes, and
    // be able to contain at least the minimum number of pairs requested
    // and no more than the maximum number requested.

    if (auto const size = end - start;
                 !(size  & 1)       &&
                  (size >= 2 * Min) &&
                  (size <= 2 * Max))
    {
      return invalidIndex(view, start, size) == size;
    }

    return false;
  }

  // Valid test cases

  static_assert(containsValidGrid(u"  AA00"));
  static_assert(containsValidGrid(u"AA00  "));
  static_assert(containsValidGrid(u" aA00Aa "));

   // Invalid test cases

  static_assert(!containsValidGrid(u""));
  static_assert(!containsValidGrid(u" A "));
  static_assert(!containsValidGrid(u"A "));
  static_assert(!containsValidGrid(u" A"));
  static_assert(!containsValidGrid(u"        "));

  // Template specifying a Maidenhead grid validator, where the minimum
  // number of acceptable pairs and maximum number of acceptable pairs
  // must be specified.
  //
  // In order to be valid, at least the minimum number of pairs must  be
  // provided, no more than the maximum must be provided, and all pairs
  // must be valid.
  //
  // Incorrect Min / Max specifications will fail to compile.

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

      if (auto const index = invalidIndex(input, 0, size);
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
  //             specify subsquare.
  //
  //   Extended: User must specify field and square, and can optionally
  //             specify subsquare, extended, ultra extended, and hyper
  //             extended.

  using StandardValidator = Validator<2,3>;
  using ExtendedValidator = Validator<2,6>;
}

#endif
