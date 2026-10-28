/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

namespace deskflow {

/**
 * @brief Axis-aligned rectangle in desktop pixel coordinates
 */
struct Rect
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;

  bool contains(int32_t px, int32_t py) const
  {
    return px >= x && px < x + w && py >= y && py < y + h;
  }

  bool operator==(const Rect &) const = default;
};

/**
 * @brief Point in desktop pixel coordinates
 */
struct Point
{
  int32_t x = 0;
  int32_t y = 0;

  bool operator==(const Point &) const = default;
};

} // namespace deskflow
