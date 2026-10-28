/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class PlacementTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void exitLeftFromUltrawide();
  void exitDownFromG9Overhang();
  void noExitWhereAnotherMonitorIsAdjacent();
  void noExitIntoEmptyRightGap();
  void noExitFromG9LeftEdge();
  void exitFromUnclippedHookPosition();
  void cornerPicksSideThatHitsPlacement();
  void noExitAwayFromEdges();
  void noPlacementsMeansNoExit();
  void clientEntryFromLeft();
  void clientEntryFromBottom();
  void entryClampedToLastPixel();
  void serverEntryRightToUltrawide();
  void serverEntryIsInsetByZone();
  void serverEntryUpToG9();
  void serverEntryLeftGoesNowhere();
  void edgeFractionHalf();
  void parseValid();
  void parseRejects();
};
