#ifndef RDP_HPP__
#define RDP_HPP__

#include <QBitArray>
#include <QPair>
#include <QPolygonF>
#include <QStack>

class RDP
{
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