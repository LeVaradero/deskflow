/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "base/Rect.h"

#include <optional>
#include <string>
#include <vector>

namespace deskflow::server::placement {

using deskflow::Point;
using deskflow::Rect;

/**
 * @brief A client placed inside the desktop coordinates of another computer
 *
 * Lets the server switch through the edges of its individual monitors
 * instead of only through the edges of its whole virtual desktop.
 */
struct Placement
{
  std::string client;    //!< canonical name of the placed client
  std::string reference; //!< computer whose desktop coordinates are used
  Rect area;             //!< area occupied by the client, in pixels of `reference`

  bool operator==(const Placement &) const = default;
};

//! Result of leaving a server monitor towards a placed client
struct ServerExit
{
  std::string client; //!< client to switch to
  Direction travel;   //!< direction of travel (Left means leaving through a left edge)
  float fraction;     //!< relative position [0,1) along the client's entry edge
};

/**
 * @brief Find a placed client to switch to from the server
 *
 * A monitor edge only counts when no other monitor of the server lies
 * just beyond it, so moving between the server's own monitors never switches.
 */
std::optional<ServerExit> findServerExit(
    const std::vector<Rect> &monitors, const std::vector<Placement> &placements, int32_t x, int32_t y,
    int32_t zoneSize
);

//! Point on the client, on the edge opposite to the direction of travel
Point clientEntryPoint(const Rect &clientShape, Direction travel, float fraction);

//! Relative position [0,1) of (x,y) along the edge crossed when moving in `travel`
float edgeFraction(const Rect &shape, Direction travel, int32_t x, int32_t y);

/**
 * @brief Find where to enter the server when leaving a placed client
 *
 * @return the entry point, moved `zoneSize` pixels inside the monitor, or
 * nothing if no server monitor lies just beyond `area` in that direction
 */
std::optional<Point> findServerEntry(
    const std::vector<Rect> &monitors, const Rect &area, Direction travel, float fraction, int32_t zoneSize
);

/**
 * @brief Parse a placement line: `<client> = <reference>(<x>, <y>, <w>, <h>)`
 *
 * @throws std::invalid_argument on bad syntax or a non-positive size
 */
Placement parsePlacement(const std::string &line);

} // namespace deskflow::server::placement
