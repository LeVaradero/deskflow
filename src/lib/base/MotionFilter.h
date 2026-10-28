/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Rect.h"

#include <cstdint>

namespace deskflow {

/**
 * @brief Test if a mouse event was computed from the cursor position before a warp
 *
 * When the cursor is warped from \p from to \p to, a hook event that was
 * computed before the warp reports a position near \p from instead of near
 * \p to.  Treating it as motion would make the cursor jump.
 */
inline bool isStaleAfterWarp(Point event, Point from, Point to)
{
  const auto distance2 = [](Point a, Point b) {
    const int64_t dx = int64_t{a.x} - b.x;
    const int64_t dy = int64_t{a.y} - b.y;
    return dx * dx + dy * dy;
  };
  return distance2(event, from) < distance2(event, to);
}

//! Test if \p p lies within \p zone pixels of an edge of \p bounds
inline bool isOnEdge(Point p, const Rect &bounds, int32_t zone)
{
  return p.x < bounds.x + zone || p.x >= bounds.x + bounds.w - zone || p.y < bounds.y + zone ||
         p.y >= bounds.y + bounds.h - zone;
}

} // namespace deskflow
