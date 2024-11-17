#include "Geodesic.hpp"
#include <algorithm>
#include <cmath>
#include "QCache"
#include "QHash"
#include "QMutex"
#include "QMutexLocker"
#include "QRegularExpression"

namespace
{
  // Distance that we consider to be 'close'.

  constexpr auto KM_CLOSE = 120.0f;

  // Epsilon value for Lat / Long comparisons.

  constexpr auto LL_EPSILON = 1.e-6f;

  // Regex that'll match a valid 4 or 6 character Maidenhead grid square,
  // We don't care about case or whitespace at this point, presuming that
  // will be fixed later -- we're being liberal about what we accept here.

  auto const regex = QRegularExpression(R"(\s*[A-R]{2}[0-9]{2}\s*|[A-R]{2}[0-9]{2}[A-R]{2}\s*)",
                     QRegularExpression::CaseInsensitiveOption);

  // Return true if the provided string matches the regex, false if it
  // doesn't. We have a more efficient path to this answer in Qt 6.5 or
  // later, but can fall back if we're compiling on 6.4.

  auto
  valid(QStringView const string)
  {
    return regex
#if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
    .match(string)
#else
    .matchView(string)
#endif
    .hasMatch();
  }

  // Given a view of up to 6 uppercase, non-whitespace ASCII bytes,
  // normalize to a Grid by inserting 'M' identifiers for any missing
  // portion of a 6-character grid identifier.

  auto
  normalize(QByteArrayView const data)
  {
    using Geodesic::Grid;

    auto const size = static_cast<Grid::size_type>(data.size());
    Grid       grid;

    std::fill_n(std::copy_n(data.begin(),
                            std::min(grid.size(), size),
                            grid.begin()), grid.size() - size, 'M');
    return grid;
  }

  // Given a pair of strings that have passed validity checking, create
  // and return normalized data.

  auto
  normalize(QStringView const originView,
            QStringView const remoteView)
  {
    auto const origin = originView.trimmed().toString().toUpper();
    auto const remote = remoteView.trimmed().toString().toUpper();

    return Geodesic::Data{origin,
                          remote,
                          normalize(origin.toLatin1()),
                          normalize(remote.toLatin1()),
                          origin.length() < 6 ||
                          remote.length() < 6};
  }

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

    Coords(Geodesic::Grid const & grid)
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
  // is an ellipsoid, flattened at the poles. Thus, there is...math.
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

  // Simplfied version of the original Fortran routine; given normalized
  // data, return the azimuth in degrees and the distance in kilometers.

  auto
  azdist(Geodesic::Data const & data)
  {
    // If they've given us the same grids, reward them appropriately.

    if (data.originGrid == data.remoteGrid) return std::make_tuple(0.0f, 0.0f);

    // Convert the grids to coordinates; literally can't fail if the
    // data has been normalized.

    auto const origin = Coords{data.originGrid};
    auto const remote = Coords{data.remoteGrid};

    // If the two grids are different, but practically on top of one
    // another, then we can't go there, because we're already there.

    if ((std::abs(origin.lat() - remote.lat()) < LL_EPSILON) &&
        (std::abs(origin.lon() - remote.lon()) < LL_EPSILON))
    {
      return std::make_tuple(0.0f, 0.0f);
    }

    // Check for antipodes; if detected, well, you can't go farther
    // away without leaving the planet; it's practically the same
    // distance in any direction, and any direction is a good one;
    // no point in calculating.

    if (auto const diffLon = std::fmod(origin.lon() - remote.lon() + 720.0f, 360.0f);
         (std::abs(diffLon      - 180.0f      ) < LL_EPSILON) &&
         (std::abs(origin.lat() + remote.lat()) < LL_EPSILON))
    {
      return std::make_tuple(0.0f, 204000.0f);
    }

    // This all looks good, determine the azimuth and distance.

    return geodist(origin, remote);
  }
}

/******************************************************************************/
// Implementation
/******************************************************************************/

namespace Geodesic
{
  // Return azimuth as a numeric string, to the nearest whole degree.
  // If the caller requests units, we'll append a degree symbol.

  QString
  Azimuth::toString(bool const units) const
  {
    if (!m_value) return QString{};

    auto value = static_cast<int>(std::round(*m_value));

    return units ? QString("%1°").arg(value)
                 : QString::number   (value);
  }

  // Return distance as a numeric string, to the nearest whole kilometer
  // or mile. If we're close and either of the grid squares that gave rise
  // to us was only 4 characters, prepend a '<' to indicate that we're close,
  // but we're not sure just how close, and the actual distance is somewhere
  // within the value.
  //
  // If the caller requests units, we'll append them.

  QString
  Distance::toString(bool const miles,
                     bool const units) const
  {
    if (!m_value) return QString{};

    auto value = static_cast<int>(std::round(miles ? *m_value / 1.609344f : *m_value));

    if      (units && m_close) return QString("<%1 %2").arg(value).arg(miles ? "mi" : "km");
    else if (units)            return QString("%1 %2" ).arg(value).arg(miles ? "mi" : "km");
    else if          (m_close) return QString("<%1"   ).arg(value);
    else                       return QString::number      (value);
  }

  // Constructor; by this point, the data will have been completely sanity
  // checked, so while we might return data that's not super useful e.g.,
  // same grid or antipodes detected, we're always going to be valid.

  Vector::Vector(Data const & data)
  {
    auto  close   = false;
    auto [az, km] = azdist(data);

    if (data.squareOnly && KM_CLOSE > km)
    {
      close = true;
      km    = KM_CLOSE;
    }

    m_azimuth =  {az};
    m_distance = {km, close};
  }

  // The geodist() function is frankly something you don't want to run more
  // than you have to. Additionally, while our contract defines the ability
  // to compute a vector between any two valid grid identifiers, the fact is
  // that the origin is going to be, practically speaking, always the local
  // station.
  //
  // Vectors get looked up a lot, so caching them is of benefit. We use a
  // two-level cache, the first level being a persistent cache of origins,
  // the second level being an ephemeral cache of remotes.
  //
  // This function is reentrant, but practically speaking, it'd be unusal
  // for this to be called from anything other than the GUI thread.

  Vector
  vector(QStringView const origin,
         QStringView const remote)
  {
    using Cache = QCache<QString, Vector>;

    static QMutex                                mutex;
    static QHash<QString, QSharedPointer<Cache>> caches;

    QMutexLocker lock(&mutex);

    // Caller is expected to hand us a lot of garbage; it's literally the
    // common case. Prior to getting too far into the weeds here, a quick
    // sanity check that what we've been handed could be expected to work.
    // If not, return a vector with invalid azimuth and invalid distance.
    // Play stupid games, win stupid prizes.

    if (!(valid(origin) &&
          valid(remote)))
    {
      return Vector();
    }

    // Input data validated; we have a winner here; at this point we are
    // going to return a valid vector; get the data by which to create it.

    auto const data = normalize(origin, remote);

    // Perform first-level cache lookup; we should practically always hit
    // on this, other than the first time we're invoked.

    if (auto cache  = caches.find(data.origin);
             cache != caches.end())
    {
      // We've hit on the first level cache; if we hit on the second, then
      // return a copy of the cached vector to the caller, and we're outta
      // here. If we miss, create a vector, store a copy of it in the cache,
      // and return the original to the caller.

      if (auto const value = (*cache)->object(data.remote))
      {
        return *value;
      } 
      else
      {
        auto const vector = Vector(data);
        (*cache)->insert(data.remote, new Vector(vector));
        return vector;
      }
    }

    // We missed on the first-level cache; first time here for this origin.
    // Create a new second-level cache a vector, storing a copy of it in the
    // cache, and then cache the cache. Return the original to the caller.

    auto       cache  = new Cache();
    auto const vector = Vector(data);

    cache->insert(data.remote, new Vector(vector));
    caches.emplace(data.origin, cache);

    return vector;
  }
}

/******************************************************************************/
