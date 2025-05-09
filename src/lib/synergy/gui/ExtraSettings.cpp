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

#include "ExtraSettings.h"

#include <QSettings>
#include <QtCore>

using namespace deskflow::gui::proxy;

namespace synergy::gui {

const auto kSerialKeySettingKey = "serialKey";
const auto kActivatedSettingKey = "activated";

ExtraSettings::ExtraSettings()
{
}

void ExtraSettings::load()
{
  const auto &system = getSystemSettings();
  const auto &user = getUserSettings();

  if (system.contains(kSerialKeySettingKey)) {
    m_serialKey = system.value(kSerialKeySettingKey).toString();
  } else if (user.contains(kSerialKeySettingKey)) {
    m_serialKey = user.value(kSerialKeySettingKey).toString();
  } else {
    qDebug("no serial key found in settings");
  }

  if (system.contains(kActivatedSettingKey)) {
    m_activated = system.value(kActivatedSettingKey).toBool();
  } else if (user.contains(kActivatedSettingKey)) {
    m_activated = user.value(kActivatedSettingKey).toBool();
  } else {
    qDebug("no activation status found in settings");
  }
}

void ExtraSettings::save()
{
  auto &system = getSystemSettings();
  auto &user = getUserSettings();

  if (system.isWritable()) {
    qDebug("saving serial key to system settings");
    system.setValue(kSerialKeySettingKey, m_serialKey);
    system.setValue(kActivatedSettingKey, m_activated);
    system.sync();
  } else {
    qDebug("not saving serial key to system settings, not writable");
  }

  qDebug("saving serial key to user settings");
  user.setValue(kSerialKeySettingKey, m_serialKey);
  user.setValue(kActivatedSettingKey, m_activated);
  user.sync();
}

} // namespace synergy::gui
