/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2024 Symless Ltd.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gui/config/Settings.h"

#include <QSettings>
#include <QString>
#include <QUuid>

namespace synergy::gui {

class ExtraSettings : public deskflow::gui::Settings
{
  Q_OBJECT
public:
  ExtraSettings() = default;
  virtual ~ExtraSettings() = default;
  void load();
  void sync();

  QString serialKey() const
  {
    return m_serialKey;
  }
  void setSerialKey(const QString &serialKey)
  {
    m_serialKey = serialKey;
  }

  bool activated() const
  {
    return m_activated;
  }
  void setActivated(bool activated)
  {
    m_activated = activated;
  }

private:
  QString m_serialKey;
  bool m_activated = false;
};

} // namespace synergy::gui
