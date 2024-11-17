#include "Geodesic.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include "QRegularExpression"

namespace
{
  using Grid = std::array<char, 6>;

  // Distances that we consider to be 'close'.

  constexpr auto CloseMiles = 75;
  constexpr auto CloseKM    = 120;

  // Regex that'll match a valid 4 or 6 character Maidenhead grid square,
  // assuming the string being validated has been trimmed fore and aft.
  // We don't care about case at this point, presuming that'll be fixed
  // during normalization -- we're being liberal in what we accept here.

  auto const regex = QRegularExpression("^[A-Z]{2}[0-9]{2}([A-X]{2})?$",
                     QRegularExpression::CaseInsensitiveOption);

  // Grid to coordinate transformation, with results exactly matching
  // those of the Fortran subroutine grid2deg().

  class Coords
  {
  private:

    // Data members

    float _lat;
    float _lon;

  public:

    // Inline Accessors

    auto lat() const { return _lat; }
    auto lon() const { return _lon; } 
    
    // Constructor

    Coords(Grid const & grid)
    : _lat([](auto const & grid)
      {
        auto const field      = -90 + 10 *  (grid[1] - 'A');
        auto const square     =             (grid[3] - '0');
        auto const subsquare  =     2.5f * ((grid[5] - 'A') + 0.5f);

        return field + square + subsquare / 60.0f;
      }(grid))
    , _lon([](auto const & grid)
      {
        auto const field      = 180 - 20 *  (grid[0] - 'A');
        auto const square     =        2 *  (grid[2] - '0');
        auto const subsquare  =        5 * ((grid[4] - 'A') + 0.5f);

        return field - square - subsquare / 60.0f;
      }(grid))
    {} 
  };

  // An exact reproduction, without the unused back azimuth, of JHT's
  // original Fortran subroutine. While in a perfect world, we could
  // use the Haversine distance, a perfect world is a sphere, and ours
  // is an ellipsoid, flatted at the poles. Thus, there is...math.
  //
  // Boy howdy, there is math.
  //
  // Commentary from the original:
  //
  //   JHT: In actual fact, I use the first two arguments for "My Location",
  //        the second two for "His location"; West longitude is positive.
  //
  //        Taken directly from:
  //
  //          Thomas, P.D., 1970, Spheroidal geodesics, reference systems,
  //          & local geometry, U.S. Naval Oceanographic Office SP-138,
  //          165 pp.
  //
  //        assumes North Latitude and East Longitude are positive
  //
  //        EpLat, EpLon = End point Lat/Long
  //        Stlat, Stlon = Start point lat/long
  //
  //        Az   = direct azimuth
  //        BAz  = reverse azimuith, discarded
  //        Dist = Dist (km);
  //        Deg  = central angle, discarded

  auto
  geodist(Coords const & P1,
          Coords const & P2)
  {
    constexpr auto AL  = 6378206.4f;        // Clarke 1866 ellipsoid
    constexpr auto BL  = 6356583.8f;
    constexpr auto D2R = 0.01745329251994f; // degrees to radians conversion factor
    constexpr auto TAU = 6.28318530718f;
    constexpr auto BOA = BL / AL;
    constexpr auto F   = 1.0f - BOA;

    if ((std::abs(P1.lat() - P2.lat()) < 0.02f) &&
        (std::abs(P1.lon() - P2.lon()) < 0.02f))
    {
      return std::make_tuple(0.0f, 0.0f);
    }

    auto const P1R   = P1.lat() * D2R;
    auto const P2R   = P2.lat() * D2R;
    auto const L1R   = P1.lon() * D2R;
    auto const L2R   = P2.lon() * D2R;
    auto const DLR   = L2R - L1R;           // Delta Longitude in Rads
    auto const T1R   = std::atan(BOA * std::tan(P1R));
    auto const T2R   = std::atan(BOA * std::tan(P2R));
    auto const TM    = (T1R + T2R) / 2.0f;
    auto const DTM   = (T2R - T1R) / 2.0f;
    auto const STM   = std::sin(TM);
    auto const CTM   = std::cos(TM);
    auto const SDTM  = std::sin(DTM);
    auto const CDTM  = std::cos(DTM);
    auto const KL    = STM * CDTM;
    auto const KK    = SDTM * CTM;
    auto const SDLMR = std::sin(DLR / 2.0f);
    auto const L     = SDTM * SDTM + SDLMR * SDLMR * (CDTM * CDTM - STM * STM);
    auto const CD    = 1.0f - 2.0f * L;
    auto const DL    = std::acos(CD);
    auto const SD    = std::sin(DL);
    auto const T     = DL / SD;
    auto const U     = 2.0f * KL * KL / (1.0f - L);
    auto const V     = 2.0f * KK * KK / L;
    auto const D     = 4.0f * T * T;
    auto const X     = U + V;
    auto const E     = -2.0f * CD;
    auto const Y     = U - V;
    auto const A     = -D * E;
    auto const FF64  = F * F / 64.0f;
    auto const dist  = AL * SD * (T - (F / 4.0f) * (T * X - Y) + FF64 * (X * (A + (T - (A + E) / 2.0f) * X) + Y * (-2.0f * D + E * Y) + D * X * Y)) / 1000.0f;
    auto const TDLPM = std::tan((DLR + (-((E * (4.0f - X) + 2.0f * Y) * ((F / 2.0f) * T + FF64 * (32.0f * T + (A - 20.0f * T) * X - 2.0f * (D + 2.0f) * Y)) / 4.0f) * std::tan(DLR))) / 2.0f);
    auto const HAPBR = std::atan2(SDTM, (CTM * TDLPM));
    auto const HAMBR = std::atan2(CDTM, (STM * TDLPM));
    auto       A1M2  = TAU + HAMBR - HAPBR;
    // auto A2M1 = TAU - HAMBR - HAPBR;

    while (A1M2 < 0.0f || A1M2 >= TAU)
    {
      if      (A1M2 < 0.0f) A1M2 += TAU;
      else if (A1M2 >= TAU) A1M2 -= TAU;
    }

    // while (A2M1 < 0.0f || A2M1 >= TAU)
    // {
    //     if      (A2M1 < 0.0f) A2M1 += TAU;
    //     else if (A2M1 >= TAU) A2M1 -= TAU;
    // }

    // Fix the mirrored coordinates

    auto const az = 360.0f - (A1M2 / D2R);
    // auto const baz = 360.f - (AIM2 / D2R);

    return std::make_tuple(az, dist);
  }

  // Assuming a trimmed, validated string representing a grid, normalize
  // to upper case and insert 'M' identifiers for any missing portion of
  // a 6-character grid identifier. Since we'll have validated using the
  // regex above, we can be confident that this will convert to ASCII.

  auto
  normalizeGrid(QString const & string)
  {
    auto const data = string.toUpper().toLatin1();
    auto const size = static_cast<Grid::size_type>(data.size());
    Grid       grid;

    std::fill_n(std::copy_n(data.begin(),
                            std::min(grid.size(), size),
                            grid.begin()), grid.size() - size, 'M');
    return grid;
  }

  // Simplfied version of the original Fortran routine; given normalized
  // origin and remote grids, return the distance in kilometers and the
  // azimuth in whole degrees. We won't reset the azimuth if we return
  // early; it's on the caller to initialize it to zero if desired.

  auto
  azdist(Grid const & originGrid,
         Grid const & remoteGrid)
  {
    if (originGrid == remoteGrid) return std::make_tuple(0.0f, 0.0f);

    auto const origin = Coords{originGrid};
    auto const remote = Coords{remoteGrid};

    constexpr auto epsilon = 1.e-6f;

    if ((std::abs(origin.lat() - remote.lat()) < epsilon) &&
        (std::abs(origin.lon() - remote.lon()) < epsilon))
    {
      return std::make_tuple(0.0f, 0.0f);
    }

    // Check for antipodes.

    if (auto const diffLon = std::fmod(origin.lon() - remote.lon() + 720.0f, 360.0f);
         (std::abs(diffLon      - 180.0f      ) < epsilon) &&
         (std::abs(origin.lat() + remote.lat()) < epsilon))
    {
      return std::make_tuple(0.0f, 204000.0f);
    }

    return geodist(origin, remote);
  }
}

/******************************************************************************/
// Implementation
/******************************************************************************/

namespace Geodesic
{
  QString
  Azimuth::toString() const
  {
    return m_value ? QString::number(*m_value) : QString{};
  }

  QString
  Distance::toString() const
  {
    if      (!m_value) return QString{};
    else if (!m_close) return QString::number(*m_value);
    else               return QString("<%1").arg(*m_value);
  }

  Vector::Vector(QString const & originGrid,
                 QString const & remoteGrid,
                 bool    const   inMiles)
  {
    auto const originGridTrimmed = originGrid.trimmed();
    auto const remoteGridTrimmed = remoteGrid.trimmed();

    if (regex.match(originGridTrimmed).hasMatch() &&
        regex.match(remoteGridTrimmed).hasMatch())
    {
      auto const [az, km] = azdist(normalizeGrid(originGridTrimmed),
                                   normalizeGrid(remoteGridTrimmed));

      auto distance = std::round(inMiles ? km / 1.609344f : km);
      auto isClose  = false;

      if (originGridTrimmed.length() < 6 ||
          remoteGridTrimmed.length() < 6)
      {
        if (auto const close = inMiles ? CloseMiles : CloseKM;
                       close > distance)
        {
          isClose  = true;
          distance = close;
        }
      }

      m_azimuth =  {static_cast<int>(std::round(az))};
      m_distance = {static_cast<int>(distance), isClose};
    }
  }
}

/******************************************************************************/
