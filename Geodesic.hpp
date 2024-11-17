#include <QString>

namespace Geodesic
{
  // Vector class, encapsulates azimuth and distance from an
  // origin grid to a remote grid.

  class Vector
  {
    // Data members

    bool m_valid    = false;
    bool m_close    = false;
    int  m_azimuth  = 0;
    int  m_distance = 0;

  public:

    // Constructor

    Vector(QString const & originGrid,
           QString const & remoteGrid,
           bool            inMiles);

    // Inline accessors

    auto isValid()  const { return m_valid;    }
    auto isClose()  const { return m_close;    }
    auto azimuth()  const { return m_azimuth;  }
    auto distance() const { return m_distance; }

    // Validity operator

    explicit operator bool () const noexcept { return m_valid; } 

    // String conversion

    QString toStringAzimuth()  const;
    QString toStringDistance() const;
  };
}