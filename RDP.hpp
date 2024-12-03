#ifndef RDP_HPP__
#define RDP_HPP__

#include <QBitArray>
#include <QPair>
#include <QPolygonF>
#include <QStack>

class RDP
{
  // This gets called approximately 10 times per second, and until
  // the associated view resizes, it's going to need exactly the
  // same amount of stack and tracking array as it did last time.
  // Throwing that away and requesting it again every 100ms isn't
  // ideal, which is why this is a functor instead of a function.

  QStack<QPair<
    qsizetype,
    qsizetype>> stack;
  QBitArray     elide;

public:

  // Process the provided polygon through the Ramer–Douglas–Peucker
  // algorithm at the requested epsilon level, modifying it in-place
  // and returning an iterator suitable for handing to polygon.erase().

  QPolygonF::iterator
  operator()(QPolygonF & polygon,
             qreal       epsilon = 2.0);
};

#endif