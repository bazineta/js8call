#ifndef __JS8
#define __JS8

#include <functional>

namespace JS8
{
  namespace SyncStats
  {
    using Candidate = std::function<void(int, float, float, float)>;
    using Processed = std::function<void(int, float, float, float)>;
  }

  using DFP = void(*)(int, float, float, float, bool);

  void decode(DFP);
}

#endif