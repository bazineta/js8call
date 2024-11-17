#include <QString>

// Distance class, encapsulates determination of distance and
// azimuth from an origin grid to a remote grid.

class Distance
{
public:

  // Constructor

  Distance(QString const & originGrid,
           QString const & remoteGrid,
           bool    const   inMiles);

  // Conversion operators; return validity and distance. These
  // are all we need to implement an ordering relation, but we
  // do so elsewhere.

  explicit operator bool () const noexcept { return m_valid;    }
           operator  int () const noexcept { return m_distance; }

  // String conversion; if valid, return computed information;
  // if invalid, an empty string.

  QString toString() const;

private:

  // Distances that we consider to be 'close'.

  static constexpr auto CloseMiles = 75;
  static constexpr auto CloseKM    = 120;

  // Data members

  int  m_azimuth  = 0;
  int  m_distance = 0;
  bool m_valid    = false;
  bool m_close    = false;
  bool m_inMiles;
};