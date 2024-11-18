#include "Geodesic.hpp"
#include "QCache"
#include "QMutex"
#include "QMutexLocker"
#include "QRegularExpression"

/******************************************************************************/
// Constants
/******************************************************************************/

namespace
{
  // Epsilon values for Lat / Long comparisons.

  constexpr auto LL_EPSILON_IDENTICAL = 0.02f;
  constexpr auto LL_EPSILON_ANTIPODES = 1.e-6f;

  // Regex that'll match a valid 4 or 6 character Maidenhead grid square.
  // We don't care about case or whitespace at this point, presuming that
  // will be fixed later -- we're being liberal about what we accept here.
  //
  // Regular expression requirements:
  //
  //   1. The 4-character format should be two letters (A-R), followed
  //      by two digits (0-9).
  //
  //   2. The 6-character format should be two letters (A-R), followed
  //      by two digits (0-9), followed by two more letters (A-X).
  //
  //   3. The grid square may have any amount of whitespace before or
  //      after.
  //
  //   4. The regular expression should be case-insensitive.

  auto const VALID = QRegularExpression(R"(\s*[A-R]{2}[0-9]{2}([A-X]{2})?\s*)",
                     QRegularExpression::CaseInsensitiveOption);
}

/******************************************************************************/
// Input Validation and Normalization
/******************************************************************************/

namespace
{
  // Return true if the provided string matches the regex, false if it
  // doesn't. Qt 6.5 or later can look at the view as-as; if compiling
  // on an earlier release, we'll have to convert to a string first.

  auto
  valid(QStringView const string)
  {
    return VALID
#if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
    .match(string)
#else
    .matchView(string)
#endif
    .hasMatch();
  }

  // Structure used to perform lookups; represents normalized, i.e.,
  // validated, trimmed fore and aft, converted to upper case, grid
  // identifiers, and an indication if either are only sufficiently
  // long to contain square, rather than subsquare, data.

  struct Data
  {
    QString origin;
    QString remote;
    bool    square;
  };

  // Given a pair of strings that have passed validity checking, create
  // and return normalized data.

  auto
  normalize(QStringView const origin,
            QStringView const remote)
  {
    auto const normalizedOrigin = origin.trimmed().toString().toUpper();
    auto const normalizedRemote = remote.trimmed().toString().toUpper();

    return Data{normalizedOrigin,
                normalizedRemote,
                normalizedOrigin.length() < 6 ||
                normalizedRemote.length() < 6};
  }
}

/******************************************************************************/
// Grid Square to Coordinates
/******************************************************************************/

namespace
{
  // Grid to coordinate transformation, with results exactly matching those
  // of the Fortran subroutine grid2deg(). Input is a 6 or 4 character grid
  // square, validated and normalized by the functions above.

  inline auto
  gridLat(QStringView const grid)
  {
    auto const m1 =                     grid[1].unicode()        - u'A';
    auto const m3 =                     grid[3].unicode()        - u'0';
    auto const m5 = (grid.size() == 6 ? grid[5].unicode() : 'M') - u'A';

    return (-90 + 10 *  m1)
         +              m3
         +   (( 2.5f * (m5 + 0.5f)) / 60.0f);
  }

  inline auto
  gridLon(QStringView const grid)
  {
    auto const m0 =                     grid[0].unicode()        - u'A';
    auto const m2 =                     grid[2].unicode()        - u'0';
    auto const m4 = (grid.size() == 6 ? grid[4].unicode() : 'M') - u'A';

    return (180 - 20 *  m0)
         -        (2 *  m2)
         -      (( 5 * (m4 + 0.5f)) / 60.0f);
  }

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

    Coords(QStringView const grid)
    : _lat(gridLat(grid))
    , _lon(gridLon(grid))
    {} 
  };
}

/******************************************************************************/
// Coordinates to Azimuth / Distance
/******************************************************************************/

namespace
{
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

    auto   A1M2  = TAU + HAMBR - HAPBR;
    while (A1M2 < 0.0f || A1M2 >= TAU)
    {
      if      (A1M2 < 0.0f) A1M2 += TAU;
      else if (A1M2 >= TAU) A1M2 -= TAU;
    }

    // auto   A2M1 = TAU - HAMBR - HAPBR;
    // while (A2M1 < 0.0f || A2M1 >= TAU)
    // {
    //     if      (A2M1 < 0.0f) A2M1 += TAU;
    //     else if (A2M1 >= TAU) A2M1 -= TAU;
    // }

    // Fix the mirrored coordinates

    auto const az = 360.0f - (A1M2 / D2R);
    // auto const baz = 360.f - (A2M1 / D2R);

    return std::make_pair(az, dist);
  }

  // Simplfied version of the original Fortran routine; given normalized
  // data, return the azimuth in degrees and the distance in kilometers.

  auto
  azdist(Data const & data)
  {
    // If they've given us the same grids, reward them appropriately.

    if (data.origin == data.remote) return std::make_pair(0.0f, 0.0f);

    // Convert the grids to coordinates.

    auto const origin = Coords{data.origin};
    auto const remote = Coords{data.remote};

    // Grids that looked different prior to conversion to coordinates
    // can nevertheless be practically on top of one another; we can't
    // go there, because we're already there.

    if ((std::abs(origin.lat() - remote.lat()) < LL_EPSILON_IDENTICAL) &&
        (std::abs(origin.lon() - remote.lon()) < LL_EPSILON_IDENTICAL))
    {
      return std::make_pair(0.0f, 0.0f);
    }

    // Check for antipodes, in which case you can't go farther away
    // without leaving the planet, it's practically the same distance
    // in any direction, and any direction is a good one; no point
    // in calculating.

    if (auto const diffLon = std::fmod(origin.lon() - remote.lon() + 720.0f, 360.0f);
         (std::abs(diffLon      - 180.0f      ) < LL_EPSILON_ANTIPODES) &&
         (std::abs(origin.lat() + remote.lat()) < LL_EPSILON_ANTIPODES))
    {
      return std::make_pair(0.0f, 204000.0f);
    }

    // Sanity checks complete; let's do some math.

    return geodist(origin, remote);
  }
}

/******************************************************************************/
// Public Implementation
/******************************************************************************/

namespace Geodesic
{
  // Return azimuth as a numeric string, to the nearest whole degree.
  // If the caller requests units, we'll append a degree symbol.

  QString
  Azimuth::toString(bool const units) const
  {
    if (!isValid()) return QString{};

    auto value = static_cast<int>(std::round(m_value));

    return units ? QString("%1°").arg(value)
                 : QString::number   (value);
  }

  // Return distance as a numeric string, to the nearest whole kilometer
  // or mile. If we're close and either of the grids that gave rise to us
  // to us was only of square, rather than subquare, magnitude, prepend a
  // '<' to indicate that we're close, but we're not sure just how close,
  // and the actual distance is somewhere within the value.
  //
  // If the caller requests units, we'll append them.

  QString
  Distance::toString(bool const miles,
                     bool const units) const
  {
    if (!isValid()) return QString{};

    auto value = static_cast<int>(std::round(miles ? m_value / 1.609344f : m_value));

    if      (units && isClose()) return QString("<%1 %2").arg(CLOSE).arg(miles ? "mi" : "km");
    else if (units)              return QString("%1 %2" ).arg(value).arg(miles ? "mi" : "km");
    else if          (isClose()) return QString("<%1"   ).arg(CLOSE);
    else                         return QString::number      (value);
  }

  // The geodist() function is frankly something you don't want to run more
  // than you have to. Additionally, while our contract defines the ability
  // to compute a vector between any two valid grid identifiers, the fact is
  // that the origin is going to be, practically speaking, always the local
  // station.
  //
  // Vectors get looked up a lot, so caching them is of benefit. We use a
  // two-level cache, the first level being a cache of origins, which will
  // be reasonably persistent, the second level being an ephemeral cache of
  // remotes.
  //
  // Note that the vector returned to the caller is theirs; it's always a
  // copy of a cached version, or a new one that we create. They should be
  // only 8 bytes in size (2 floats); so this should be quite efficient.
  //
  // This function is reentrant, but practically speaking, it'd be unusual
  // for this to be called from anything other than the GUI thread.

  Vector
  vector(QStringView const origin,
         QStringView const remote)
  {
    using Cache = QCache<QString, Vector>;

    static QMutex                 mutex;
    static QCache<QString, Cache> caches;

    QMutexLocker lock(&mutex);

    // Caller is expected to hand us a lot of garbage; it's literally the
    // common case. Prior to getting too far into the weeds here, a quick
    // sanity check that what we've been handed could be expected to work.
    // If not, return a vector with invalid azimuth and invalid distance.
    // Play stupid games, win stupid prizes.

    if (!valid(origin) || 
        !valid(remote))
    {
      return Vector();
    }

    // Input data validated; we have a winner here; at this point we are
    // going to return a valid vector; get the data by which to create it.

    auto const data = normalize(origin, remote);

    // Perform first-level cache lookup; we should practically always hit
    // on this, other than the first time we're invoked.

    if (auto cache = caches.object(data.origin))
    {
      // We've hit on the first level cache; if we hit on the second, then
      // return a copy of the cached vector to the caller, and we're outta
      // here. If we miss, create a vector, store a copy of it in the cache,
      // and return the original to the caller.

      if (auto const value = cache->object(data.remote))
      {
        return *value;
      } 
      else
      {
        auto const vector = Vector(azdist(data), data.square);
        cache->insert(data.remote, new Vector(vector));
        return vector;
      }
    }

    // We missed on the first-level cache; first time here for this origin.
    // Create a new second-level cache and a vector, storing a copy of the
    // vector in the cache, and then cache the cache. Return the original
    // vector to the caller.

    auto       cache  = new Cache();
    auto const vector = Vector(azdist(data), data.square);

    cache->insert(data.remote, new Vector(vector));
    caches.insert(data.origin, cache);

    return vector;
  }
}

/******************************************************************************/
