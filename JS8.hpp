#ifndef __JS8
#define __JS8

namespace JS8
{
  using DFP = void(*)(int, float, float, float, bool);

  void decode(DFP);
}

#endif