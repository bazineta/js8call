#include <optional>
#include <QString>

namespace Geodesic
{
  // Azimuth class, describes an azimuth in degrees. Created via
  // interpolation of Maidenhead grid coordinates, and as such
  // will be invalid if interpolation failed, typically due to
  // bad coordinates.

  class Azimuth
  {
    // Data members

    std::optional<float> m_value;

    // Constructors

    Azimuth() = default;
    Azimuth(float const value)
    : m_value {value}
    {}

    // Allow construction only by Vector.

    friend class Vector;

  public:

    // Allow copying, moving, and assignment by anyone.

    Azimuth            (Azimuth const & )          = default;
    Azimuth & operator=(Azimuth const & )          = default;
    Azimuth            (Azimuth       &&) noexcept = default;
    Azimuth & operator=(Azimuth       &&) noexcept = default;

    // Conversion operators; return validity and value. These
    // are all we need to implement an ordering relation, but
    // we do so elsewhere.

    explicit operator  bool () const noexcept { return m_value.has_value();    }  
             operator float () const noexcept { return m_value.value_or(0.0f); }

    // String conversion, to the nearest whole degree; always
    // succeeds, returning an empty string if invalid.

    QString toString() const;
  };

  // Distance class, describes a distance in kilometers. Created via
  // interpolation of Maidenhead grid coordinates, and as such will
  // be invalid if interpolation failed, typically due to bad coordinates.
  //
  // May additionally be defined as 'close', meaning that either of the
  // grids provided was only 4 characters and the computed distance was
  // short, so we know it's close, but not just how close. In this case,
  // the value will be a non-zero minimum constant, and string conversion
  // will prepend a '<'.
  //
  // While distances are stored internally only in kilometers, caller may
  // request string conversion in terms of statute miles.

  class Distance
  {
    // Data members

    std::optional<float>  m_value;
    bool                  m_close = false;

    // Constructors

    Distance() = default;
    Distance(float const value,
             bool  const close)
    : m_value {value}
    , m_close {close}
    {}

    // Allow construction only by Vector.

    friend class Vector;

  public:

    // Allow copying, moving, and assignment by anyone.

    Distance            (Distance const & )          = default;
    Distance & operator=(Distance const & )          = default;
    Distance            (Distance       &&) noexcept = default;
    Distance & operator=(Distance       &&) noexcept = default;

    // Conversion operators; return validity and value. These
    // are all we need to implement an ordering relation, but
    // we do so elsewhere.

    explicit operator  bool () const noexcept { return m_value.has_value();    }
             operator float () const noexcept { return m_value.value_or(0.0f); }

    // Inline Accessors

    auto isValid() const { return m_value.has_value();    }
    auto isClose() const { return m_close;                }
    auto value()   const { return m_value.value_or(0.0f); }

    // String conversion, to the nearest whole kilometer or mile,
    // always succeeds, returning an empty string if invalid.

    QString toString(bool miles) const;
  };

  // Vector class, aggregate of azimuth and distance from an
  // origin grid to a remote grid.

  class Vector
  {
    // Data members

    Azimuth  m_azimuth;
    Distance m_distance;

  public:

    // Constructor

    Vector(QString const & originGrid,
           QString const & remoteGrid);

    // Inline accessors

    Azimuth const  & azimuth()  const { return m_azimuth;  }
    Distance const & distance() const { return m_distance; }
  };
}