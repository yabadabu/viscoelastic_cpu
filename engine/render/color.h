#ifndef INC_RENDER_COLORS_H_
#define INC_RENDER_COLORS_H_

namespace Color {
  constexpr VEC4 Red(1, 0, 0, 1);
  constexpr VEC4 Green(0, 1, 0, 1);
  constexpr VEC4 Blue(0, 0, 1, 1);
  constexpr VEC4 White(1, 1, 1, 1);
  constexpr VEC4 Black(0, 0, 0, 1);
  constexpr VEC4 Yellow(1, 1, 0, 1);
  constexpr VEC4 Magenta(1, 0, 1, 1);
  constexpr VEC4 Grey(0.5f, .5f, .5f, 1);
  constexpr VEC4 Cyan(0, 1, 1, 1);
  constexpr VEC4 Orange(1, 165.0f / 255.0f, 0, 1);
  constexpr VEC4 Pink(1.0, 192.0f / 255.0f, 203.0f / 255.0f, 1);
  constexpr VEC4 DeepPink(1.0, 20.0f / 255.0f, 147.0f / 255.0f, 1);
};

#endif

