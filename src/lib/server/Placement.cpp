/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Placement.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace deskflow::server::placement {

namespace {

std::vector<Rect>::const_iterator findMonitor(const std::vector<Rect> &monitors, int32_t x, int32_t y)
{
  return std::find_if(monitors.begin(), monitors.end(), [x, y](const Rect &m) { return m.contains(x, y); });
}

bool isOnMonitor(const std::vector<Rect> &monitors, int32_t x, int32_t y)
{
  return findMonitor(monitors, x, y) != monitors.end();
}

bool isHorizontal(Direction travel)
{
  return travel == Direction::Left || travel == Direction::Right;
}

float clampFraction(float fraction)
{
  return std::clamp(fraction, 0.0f, std::nextafter(1.0f, 0.0f));
}

// offset of `fraction` along a span of `length` pixels, kept inside the span
int32_t offsetAlong(float fraction, int32_t length)
{
  const auto offset = static_cast<int32_t>(std::floor(static_cast<double>(fraction) * length));
  return std::clamp(offset, 0, length - 1);
}

std::string trim(const std::string &s)
{
  const auto first = s.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t");
  return s.substr(first, last - first + 1);
}

bool isName(const std::string &s)
{
  return !s.empty() && s.find_first_of(" \t(),=") == std::string::npos;
}

int32_t parseNumber(const std::string &text)
{
  const auto s = trim(text);
  int32_t value = 0;
  const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || error != std::errc() || end != s.data() + s.size()) {
    throw std::invalid_argument("invalid number \"" + s + "\" in placement");
  }
  return value;
}

} // namespace

std::optional<ServerExit> findServerExit(
    const std::vector<Rect> &monitors, const std::vector<Placement> &placements, int32_t x, int32_t y,
    int32_t zoneSize
)
{
  const auto monitor = findMonitor(monitors, x, y);
  if (placements.empty() || monitor == monitors.end()) {
    return std::nullopt;
  }
  const Rect &m = *monitor;

  struct Side
  {
    Direction travel;
    bool inZone;
    int32_t px;
    int32_t py;
  };
  const std::array<Side, 4> sides{{
      {Direction::Left, x < m.x + zoneSize, m.x - 1, y},
      {Direction::Right, x >= m.x + m.w - zoneSize, m.x + m.w, y},
      {Direction::Top, y < m.y + zoneSize, x, m.y - 1},
      {Direction::Bottom, y >= m.y + m.h - zoneSize, x, m.y + m.h},
  }};

  for (const auto &side : sides) {
    if (!side.inZone || isOnMonitor(monitors, side.px, side.py)) {
      continue;
    }
    for (const auto &placement : placements) {
      const Rect &a = placement.area;
      if (!a.contains(side.px, side.py)) {
        continue;
      }
      const float fraction = isHorizontal(side.travel) ? static_cast<float>(y - a.y) / static_cast<float>(a.h)
                                                       : static_cast<float>(x - a.x) / static_cast<float>(a.w);
      return ServerExit{placement.client, side.travel, clampFraction(fraction)};
    }
  }
  return std::nullopt;
}

Point clientEntryPoint(const Rect &clientShape, Direction travel, float fraction)
{
  const Rect &s = clientShape;
  switch (travel) {
  case Direction::Left:
    return {s.x + s.w - 1, s.y + offsetAlong(fraction, s.h)};
  case Direction::Right:
    return {s.x, s.y + offsetAlong(fraction, s.h)};
  case Direction::Top:
    return {s.x + offsetAlong(fraction, s.w), s.y + s.h - 1};
  case Direction::Bottom:
    return {s.x + offsetAlong(fraction, s.w), s.y};
  default:
    return {s.x, s.y};
  }
}

float edgeFraction(const Rect &shape, Direction travel, int32_t x, int32_t y)
{
  const float fraction = isHorizontal(travel) ? static_cast<float>(y - shape.y) / static_cast<float>(shape.h)
                                              : static_cast<float>(x - shape.x) / static_cast<float>(shape.w);
  return clampFraction(fraction);
}

std::optional<Point> findServerEntry(
    const std::vector<Rect> &monitors, const Rect &area, Direction travel, float fraction, int32_t zoneSize
)
{
  Point p;
  switch (travel) {
  case Direction::Left:
    p = {area.x - 1, area.y + offsetAlong(fraction, area.h)};
    break;
  case Direction::Right:
    p = {area.x + area.w, area.y + offsetAlong(fraction, area.h)};
    break;
  case Direction::Top:
    p = {area.x + offsetAlong(fraction, area.w), area.y - 1};
    break;
  case Direction::Bottom:
    p = {area.x + offsetAlong(fraction, area.w), area.y + area.h};
    break;
  default:
    return std::nullopt;
  }

  const auto monitor = findMonitor(monitors, p.x, p.y);
  if (monitor == monitors.end()) {
    return std::nullopt;
  }
  const Rect &m = *monitor;

  // move inside the monitor so the cursor doesn't land in the jump zone
  switch (travel) {
  case Direction::Left:
    p.x -= zoneSize;
    break;
  case Direction::Right:
    p.x += zoneSize;
    break;
  case Direction::Top:
    p.y -= zoneSize;
    break;
  default:
    p.y += zoneSize;
    break;
  }
  p.x = std::clamp(p.x, m.x, m.x + m.w - 1);
  p.y = std::clamp(p.y, m.y, m.y + m.h - 1);
  return p;
}

Placement parsePlacement(const std::string &line)
{
  const auto equals = line.find('=');
  if (equals == std::string::npos) {
    throw std::invalid_argument("missing '=' in placement");
  }
  const auto client = trim(line.substr(0, equals));
  const auto value = trim(line.substr(equals + 1));

  const auto open = value.find('(');
  if (open == std::string::npos || value.empty() || value.back() != ')') {
    throw std::invalid_argument("expected <computer>(<x>, <y>, <width>, <height>) in placement");
  }
  const auto reference = trim(value.substr(0, open));
  if (!isName(client) || !isName(reference)) {
    throw std::invalid_argument("invalid computer name in placement");
  }

  std::vector<int32_t> numbers;
  const auto args = value.substr(open + 1, value.size() - open - 2);
  std::string::size_type start = 0;
  while (true) {
    const auto comma = args.find(',', start);
    numbers.push_back(parseNumber(args.substr(start, comma - start)));
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  if (numbers.size() != 4) {
    throw std::invalid_argument("placement needs exactly 4 numbers: x, y, width, height");
  }
  if (numbers[2] <= 0 || numbers[3] <= 0) {
    throw std::invalid_argument("placement width and height must be positive");
  }
  return Placement{client, reference, Rect{numbers[0], numbers[1], numbers[2], numbers[3]}};
}

} // namespace deskflow::server::placement
