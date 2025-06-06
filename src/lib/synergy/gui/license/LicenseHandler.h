/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2015 Symless Ltd.
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

#include "synergy/gui/AppTime.h"
#include "synergy/gui/ExtraSettings.h"
#include "synergy/gui/license/LicenseActivator.h"
#include "synergy/license/License.h"
#include "synergy/license/Product.h"

class AppConfig;
class QMainWindow;
class QDialog;
class QCheckBox;
class QRadioButton;

namespace deskflow::gui {
class CoreProcess;
}

/**
 * @brief A convenience wrapper for `License` that provides Qt signals, etc.
 */
class LicenseHandler : public QObject
{
  Q_OBJECT

  using License = synergy::license::License;
  using SerialKey = synergy::license::SerialKey;

public:
  enum class SetSerialKeyResult
  {
    kSuccess,
    kFatal,
    kUnchanged,
    kInvalid,
    kExpired
  };

  explicit LicenseHandler();

  static LicenseHandler &instance()
  {
    static LicenseHandler instance;
    return instance;
  }

  void handleMainWindow(QMainWindow *mainWindow, AppConfig *appConfig, deskflow::gui::CoreProcess *coreProcess);
  bool handleAppStart();
  void handleSettings(
      QDialog *parent, QCheckBox *enableTls, QCheckBox *invertConnection, QRadioButton *systemScope,
      QRadioButton *userScope
  ) const;
  void handleVersionCheck(QString &versionUrl);
  bool handleCoreStart();
  bool loadSettings();
  void saveSettings();
  const License &license() const;
  Product::Edition productEdition() const;
  QString productName() const;
  SetSerialKeyResult setLicense(const QString &hexString, bool allowExpired = false);
  void clampFeatures();
  void disable();

  bool isEnabled() const
  {
    return m_enabled;
  }

private:
  void checkTlsCheckBox(QDialog *parent, QCheckBox *checkBoxEnableTls, bool showDialog) const;
  void checkInvertConnectionCheckBox(QDialog *parent, QCheckBox *checkBoxInvertConnection, bool showDialog) const;
  void updateWindowTitle() const;
  bool showSerialKeyDialog();
  bool check();

  bool m_enabled = true;
  synergy::gui::AppTime m_time;
  License m_license = License::invalid();
  synergy::gui::ExtraSettings m_settings;
  synergy::gui::license::LicenseActivator m_activator;
  QMainWindow *m_pMainWindow = nullptr;
  AppConfig *m_pAppConfig = nullptr;
  deskflow::gui::CoreProcess *m_pCoreProcess = nullptr;
};
