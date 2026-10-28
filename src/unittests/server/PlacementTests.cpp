/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "PlacementTests.h"

#include "base/MotionFilter.h"
#include "server/Placement.h"

#include <cmath>
#include <stdexcept>

using namespace deskflow::server::placement;

namespace {
// real setup: Samsung G9 on top, 21:9 centred below it, laptop in the left gap
const Rect kG9{-1292, -1440, 5120, 1440};
const Rect kUltrawide{0, 0, 2560, 1080};
const std::vector<Rect> kMonitors{kG9, kUltrawide};
const Placement kLaptop{"LAPTOPDUFLEX", "Corsair", {-1292, 0, 1292, 1080}};
const std::vector<Placement> kPlacements{kLaptop};
const Rect kLaptopShape{0, 0, 1920, 1080};

bool approx(float a, float b)
{
  return std::abs(a - b) < 1e-3f;
}
} // namespace

void PlacementTests::exitLeftFromUltrawide()
{
  const auto hit = findServerExit(kMonitors, kPlacements, 0, 540, 1);
  QVERIFY(hit.has_value());
  QCOMPARE(hit->client, std::string("LAPTOPDUFLEX"));
  QVERIFY(hit->travel == Direction::Left);
  QVERIFY(approx(hit->fraction, 0.5f));
}

void PlacementTests::exitDownFromG9Overhang()
{
  const auto hit = findServerExit(kMonitors, kPlacements, -646, -1, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Bottom);
  QVERIFY(approx(hit->fraction, 0.5f));
}

void PlacementTests::noExitWhereAnotherMonitorIsAdjacent()
{
  QVERIFY(!findServerExit(kMonitors, kPlacements, 1000, -1, 1).has_value());
}

void PlacementTests::noExitIntoEmptyRightGap()
{
  QVERIFY(!findServerExit(kMonitors, kPlacements, 3000, -1, 1).has_value());
}

void PlacementTests::noExitFromG9LeftEdge()
{
  QVERIFY(!findServerExit(kMonitors, kPlacements, -1292, -700, 1).has_value());
}

void PlacementTests::exitFromUnclippedHookPosition()
{
  // low level hooks report the position before windows clips the cursor
  auto hit = findServerExit(kMonitors, kPlacements, -7, 540, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Left);
  QVERIFY(approx(hit->fraction, 0.5f));

  hit = findServerExit(kMonitors, kPlacements, -646, 4, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Bottom);
  QVERIFY(approx(hit->fraction, 0.5f));
}

void PlacementTests::cornerPicksSideThatHitsPlacement()
{
  const auto hit = findServerExit(kMonitors, kPlacements, -1292, -1, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Bottom);
  QVERIFY(approx(hit->fraction, 0.0f));
}

void PlacementTests::noExitAwayFromEdges()
{
  QVERIFY(!findServerExit(kMonitors, kPlacements, 1280, 540, 1).has_value());
}

void PlacementTests::noPlacementsMeansNoExit()
{
  QVERIFY(!findServerExit(kMonitors, {}, 0, 540, 1).has_value());
}

void PlacementTests::clientEntryFromLeft()
{
  QCOMPARE(clientEntryPoint(kLaptopShape, Direction::Left, 0.5f), (Point{1919, 540}));
}

void PlacementTests::clientEntryFromBottom()
{
  QCOMPARE(clientEntryPoint(kLaptopShape, Direction::Bottom, 0.25f), (Point{480, 0}));
}

void PlacementTests::entryClampedToLastPixel()
{
  QCOMPARE(clientEntryPoint(kLaptopShape, Direction::Left, 0.99999f).y, 1079);
  const auto entry = findServerEntry(kMonitors, kLaptop.area, Direction::Right, 0.99999f, 1);
  QVERIFY(entry.has_value());
  QCOMPARE(entry->y, 1079);
}

void PlacementTests::serverEntryRightToUltrawide()
{
  const auto entry = findServerEntry(kMonitors, kLaptop.area, Direction::Right, 0.5f, 1);
  QVERIFY(entry.has_value());
  QCOMPARE(*entry, (Point{1, 540}));
}

void PlacementTests::serverEntryIsInsetByZone()
{
  const auto entry = findServerEntry(kMonitors, kLaptop.area, Direction::Right, 0.5f, 5);
  QVERIFY(entry.has_value());
  QCOMPARE(*entry, (Point{5, 540}));
}

void PlacementTests::serverEntryUpToG9()
{
  const auto entry = findServerEntry(kMonitors, kLaptop.area, Direction::Top, 0.5f, 1);
  QVERIFY(entry.has_value());
  QCOMPARE(*entry, (Point{-646, -2}));
}

void PlacementTests::serverEntryLeftGoesNowhere()
{
  QVERIFY(!findServerEntry(kMonitors, kLaptop.area, Direction::Left, 0.5f, 1).has_value());
  QVERIFY(!findServerEntry(kMonitors, kLaptop.area, Direction::Bottom, 0.5f, 1).has_value());
}

void PlacementTests::edgeFractionHalf()
{
  QVERIFY(approx(edgeFraction(kLaptopShape, Direction::Right, 1920, 540), 0.5f));
}

void PlacementTests::parseValid()
{
  QCOMPARE(parsePlacement("\tLAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)"), kLaptop);
}

void PlacementTests::parseRejects()
{
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("A = B(1,2,3)"));
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("A = B(0,0,0,10)"));
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("A = B(0,0,10,-1)"));
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("A B(0,0,1,1)"));
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("A = B(x,0,1,1)"));
  QVERIFY_THROWS_EXCEPTION(std::invalid_argument, parsePlacement("= B(0,0,1,1)"));
}

namespace {
// laptop wider than the G9 overhang, sticking out on the left, with a gap before the 21:9
const Placement kGapLaptop{"LAPTOPDUFLEX", "Corsair", {-1900, 0, 1500, 1080}};
} // namespace

void PlacementTests::exitLeftAcrossGap()
{
  const auto hit = findServerExit(kMonitors, {kGapLaptop}, 0, 540, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Left);
  QVERIFY(approx(hit->fraction, 0.5f));
}

void PlacementTests::exitDownOnlyAbovePlacement()
{
  const auto hit = findServerExit(kMonitors, {kGapLaptop}, -1000, -1, 1);
  QVERIFY(hit.has_value());
  QVERIFY(hit->travel == Direction::Bottom);
  QVERIFY(approx(hit->fraction, 0.6f));

  // nothing below the gap between the laptop and the 21:9
  QVERIFY(!findServerExit(kMonitors, {kGapLaptop}, -200, -1, 1).has_value());
}

void PlacementTests::serverEntryAcrossGap()
{
  const auto right = findServerEntry(kMonitors, kGapLaptop.area, Direction::Right, 0.5f, 1);
  QVERIFY(right.has_value());
  QCOMPARE(*right, (Point{1, 540}));

  const auto up = findServerEntry(kMonitors, kGapLaptop.area, Direction::Top, 0.6f, 1);
  QVERIFY(up.has_value());
  QCOMPARE(*up, (Point{-1000, -2}));

  // the part of the laptop sticking out on the left has nothing above it
  QVERIFY(!findServerEntry(kMonitors, kGapLaptop.area, Direction::Top, 0.1f, 1).has_value());
}

void PlacementTests::staleMotionAfterWarp()
{
  // from a real log: left at -846,6, warped to 1280,540, then events at 59,76 and 59,100
  QVERIFY(deskflow::isStaleAfterWarp({59, 76}, {-846, 6}, {1280, 540}));
  QVERIFY(deskflow::isStaleAfterWarp({59, 100}, {-846, 6}, {1280, 540}));
  QVERIFY(!deskflow::isStaleAfterWarp({1206, 570}, {-846, 6}, {1280, 540}));
  QVERIFY(deskflow::isStaleAfterWarp({-3, 540}, {0, 540}, {1280, 540}));
  QVERIFY(!deskflow::isStaleAfterWarp({1200, 540}, {0, 540}, {1280, 540}));
}

void PlacementTests::leavePointOnDesktopEdge()
{
  const Rect desktop{-1292, -1440, 5120, 2520};
  QVERIFY(deskflow::isOnEdge({-1292, -700}, desktop, 1));
  QVERIFY(deskflow::isOnEdge({3827, 0}, desktop, 1));
  QVERIFY(!deskflow::isOnEdge({-846, 6}, desktop, 1));
  QVERIFY(!deskflow::isOnEdge({0, 540}, desktop, 1));
}

QTEST_MAIN(PlacementTests)
