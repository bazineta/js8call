#include "RDP.hpp"
#include <cmath>
#include <utility>

/******************************************************************************/
// Implementation
/******************************************************************************/

// We'll typically end up with a ton of points to draw for the spectrum,
// and some simplification is worthwhile; use the Ramer–Douglas–Peucker
// algorithm to reduce to a smaller number of points.
//
// We'll modify the inbound polygon in place, such that anything we want
// to keep is at the start of the polygon and anything we want to omit
// is at the end, returning an iterator to the new end, i.e., the point
// one past the last point we want to keep.
//
// Our goal here is to avoid reallocations. Since we're at worst going to
// be leaving this the same size, we should be able to work with what we
// have already.
//
// Note that this is a functor; it's serially reusable, but not reentrant.
// Call it from one thread only. In practical use, that's not expected to
// be a problem, and it allows us to reuse allocated memory in a serial
// manner, rather than requesting it and freeing it constantly.

QPolygonF::iterator
RDP::operator()(QPolygonF & polygon,
                qreal const epsilon)
{
  // There's no point in proceeding with less than 3 points.

  if (polygon.size() < 3) return polygon.end();

  // Prime our array such that all points are initially in play, and
  // our stack to consider the full span; run the stack machine until
  // it empties.

  elide.clear();
  elide.resize(polygon.size());
  stack.push({0, polygon.size() - 1});

  while (!stack.isEmpty())
  {
    auto const [
      index1,
      index2
    ] = stack.pop();

    // Create a theoretical line between the first and last points
    // in the span we're presently considering; compute the vector
    // components and the line length.

    auto const & p1 = polygon[index1];
    auto const & p2 = polygon[index2];
    auto const   dx = p2.x() - p1.x();
    auto const   dy = p2.y() - p1.y();
    auto const   ll = std::hypot(dx, dy);

    // Find the point within the span at the largest perpendicular
    // distance from the line.

    auto  index = index1;
    qreal dMax  = 0.0;

    for (auto i = index1 + 1;
              i < index2;
            ++i)
    {
      // We want to consider this point only if hasn't already been
      // marked for death. If it's still in play, see if it's got a
      // larger perpendicular distance from the line.

      if (!elide.at(i))
      {
        auto const & point = polygon[i];

        if (auto const d = std::abs(dy * (point.x() - p1.x()) -
                                    dx * (point.y() - p1.y())) / ll;
                        d > dMax)
        {
          index = i;
          dMax  = d;
        }
      }
    }

    // If the max distance is above epsilon, then we have to keep
    // working the problem. If not, cull the indices of points that
    // are not relevant to the result, i.e., everything but for the
    // first and last.

    if (dMax > epsilon)
    {
      stack.push({index1, index});
      stack.push({index, index2});
    }
    else
    {
      for (auto i = index1 + 1;
                i < index2;
              ++i)
      {
        elide.setBit(i);
      }
    }
  }

  // Our array now contains bits set to true for every point that
  // should be removed, false for those that should be kept. Move
  // everything we want to keep to the front and return the first
  // element to remove.

  auto first = polygon.begin();

  for (qsizetype i = 0; i < polygon.size(); ++i)
  {
    if (!elide.at(i)) *first++ = std::move(polygon[i]);
  }

  return first;
}

/******************************************************************************/
