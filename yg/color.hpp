#pragma once

#include "../std/stdint.hpp"

namespace yg
{
  struct Color
  {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    Color(unsigned char r1, unsigned char g1, unsigned char b1, unsigned char a1);
    Color();
    Color(Color const & p);
    Color const & operator=(Color const & p);

    static Color const fromARGB(uint32_t _c);
    static Color const fromXRGB(uint32_t _c, unsigned char _a = 0);

    Color const & operator /= (unsigned k);
  };

  bool operator < (Color const & l, Color const & r);
  bool operator > (Color const & l, Color const & r);
  bool operator !=(Color const & l, Color const & r);
  bool operator ==(Color const & l, Color const & r);

  inline int redFromARGB(uint32_t c)   {return (c >> 16) & 0xFF;}
  inline int greenFromARGB(uint32_t c) {return (c >> 8) & 0xFF; }
  inline int blueFromARGB(uint32_t c)  {return (c & 0xFF);      }
  inline int alphaFromARGB(uint32_t c) {return (c >> 24) & 0xFF;}
}
