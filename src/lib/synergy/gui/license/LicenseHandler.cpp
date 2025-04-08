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

#include "LicenseHandler.h"

#include "ActivationDialog.h"
#include "dialogs/UpgradeDialog.h"
#include "gui/config/AppConfig.h"
#include "gui/styles.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/license/license_utils.h"
#include "synergy/license/Product.h"

#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QMainWindow>
#include <QMenuBar>
#include <QObject>
#include <QProcessEnvironment>
#include <QRadioButton>
#include <QTimer>
#include <QtCore>
#include <chrono>

using namespace std::chrono;
using namespace synergy::gui::license;
using License = synergy::license::License;

LicenseHandler::LicenseHandler()
{
  m_enabled = synergy::gui::license::isActivationEnabled();
}

bool LicenseHandler::handleStart(QMainWindow *parent, AppConfig *appConfig)
{
  if (!m_enabled) {
    qDebug("license handler disabled, skipping start handler");
    parent->setWindowTitle(SYNERGY_PRODUCT_NAME);
    return true;
  }

  m_mainWindow = parent;
  m_appConfig = appConfig;

  if (m_mainWindow == nullptr) {
    qFatal("main window not set");
  }

  if (m_appConfig == nullptr) {
    qFatal("app config not set");
  }

  updateWindowTitle();

  const auto serialKeyAction = new QAction("Change serial key", parent);
  QObject::connect(serialKeyAction, &QAction::triggered, [this] { showActivationDialog(); });

  const auto licenseMenu = new QMenu("License");
  licenseMenu->addAction(serialKeyAction);
  parent->menuBar()->addAction(licenseMenu->menuAction());

  if (!loadSettings()) {
    qCritical("failed to load license settings");
    return false;
  }

  if (m_license.isExpired()) {
    qWarning("license is expired, showing activation dialog");
    return showActivationDialog();
  }

  if (!m_license.isValid()) {
    qDebug("license not valid, showing activation dialog");
    return showActivationDialog();
  }

  qDebug("license is valid, continuing with start");
  updateWindowTitle();
  clampFeatures(false);
  return true;
}

void LicenseHandler::handleSettings(
    QDialog *parent, QCheckBox *enableTls, QCheckBox *invertConnection, QRadioButton *systemScope,
    QRadioButton *userScope
) const
{
  if (!m_enabled) {
    qDebug("license handler disabled, skipping settings handler");
    return;
  }

  const auto onTlsToggle = [this, parent, enableTls] {
    qDebug("tls checkbox toggled");
    checkTlsCheckBox(parent, enableTls, true);
  };
  QObject::connect(enableTls, &QCheckBox::toggled, onTlsToggle);

  const auto onInvertConnectionToggle = [this, parent, invertConnection] {
    qDebug("invert connection checkbox toggled");
    checkInvertConnectionCheckBox(parent, invertConnection, true);
  };
  QObject::connect(invertConnection, &QCheckBox::toggled, onInvertConnectionToggle);

  const auto onSystemScopeToggle = [this, parent, systemScope, userScope] {
    qDebug("system scope radio button toggled");
    checkSettingsScopeRadioButton(parent, systemScope, userScope, true);
  };
  QObject::connect(systemScope, &QRadioButton::toggled, onSystemScopeToggle);

  checkTlsCheckBox(parent, enableTls, false);
  checkInvertConnectionCheckBox(parent, invertConnection, false);
  checkSettingsScopeRadioButton(parent, systemScope, userScope, false);
}

void LicenseHandler::handleVersionCheck(QString &versionUrl)
{
  if (!m_enabled) {
    qDebug("license handler disabled, skipping version check handler");
    return;
  }

  const auto edition = license().productEdition();
  if (edition == Product::Edition::kBusiness) {
    versionUrl.append("/business");
  } else {
    versionUrl.append("/personal");
  }
}

bool LicenseHandler::handleCoreStart(deskflow::gui::CoreProcess *coreProcess)
{
  using namespace synergy::gui;
  using namespace deskflow::gui;

  m_coreProcess = coreProcess;

  if (!m_enabled) {
    qDebug("license handler disabled, skipping core start handler");
    return true;
  }

  if (m_appConfig == nullptr) {
    qFatal("app config not set");
  }

  if (m_mainWindow == nullptr) {
    qFatal("main window not set");
  }

  if (m_settings.activated()) {
    qDebug("license is activated, starting core");
    return true;
  }

  if (m_license.serialKey().isOffline) {
    qDebug("offline serial key, starting core");
    return true;
  }

  disconnect(&m_activator, &LicenseActivator::activationFailed, this, nullptr);
  connect(&m_activator, &LicenseActivator::activationFailed, this, [this](const QString &message) {
    QString fullMessage = QString(
                              "<p>There was a problem activating your license.</p>"
                              R"(<p>Please <a href="%1" style="color: %2">contact us</a> )"
                              "and let us know this message:</p>"
                              "%3"
    )
                              .arg(kUrlContact)
                              .arg(kColorSecondary)
                              .arg(message);
    QMessageBox::warning(m_mainWindow, "Activation failed", fullMessage);
  });

  disconnect(&m_activator, &LicenseActivator::activationSucceeded, this, nullptr);
  connect(&m_activator, &LicenseActivator::activationSucceeded, this, [this, coreProcess] {
    qDebug("license activation succeeded, saving settings");
    m_settings.setActivated(true);
    m_settings.save();

    qDebug("resuming core process after activation");
    coreProcess->start();
  });

  qInfo("activating license");
  const auto serialKey = QString::fromStdString(m_license.serialKey().hexString);
  m_activator.activate({serialKey, m_appConfig->serverGroupChecked()});

  return false;
}

bool LicenseHandler::loadSettings()
{
  using enum SetSerialKeyResult;

  m_settings.load();

  const auto serialKey = m_settings.serialKey();
  if (!serialKey.isEmpty()) {
    const auto result = setLicense(m_settings.serialKey(), true);
    if (result != kSuccess && result != kUnchanged) {
      qWarning("set serial key failed, showing activation dialog");
      return showActivationDialog();
    }
  }

  return true;
}

void LicenseHandler::saveSettings()
{
  const auto hexString = m_license.serialKey().hexString;
  m_settings.setSerialKey(QString::fromStdString(hexString));
  m_settings.save();
}

bool LicenseHandler::showActivationDialog()
{
  ActivationDialog dialog(m_mainWindow, *m_appConfig, *this);
  const auto result = dialog.exec();
  if (result == QDialog::Accepted) {
    saveSettings();
    updateWindowTitle();
    clampFeatures(true);

    if (m_coreProcess != nullptr && m_coreProcess->isStarted()) {
      qDebug("restarting core on activation dialog accept");
      m_coreProcess->restart();
    }

    qDebug("license activation dialog accepted");
    return true;
  } else {
    qWarning("license activation dialog declined");
    return false;
  }
}

void LicenseHandler::updateWindowTitle() const
{
  const auto productName = QString::fromStdString(m_license.productName());
  qDebug("updating main window title: %s", qPrintable(productName));
  m_mainWindow->setWindowTitle(productName);
}

void LicenseHandler::checkTlsCheckBox(QDialog *parent, QCheckBox *checkBoxEnableTls, bool showDialog) const
{
  if (!m_license.isTlsAvailable() && checkBoxEnableTls->isChecked()) {
    qDebug("tls not available, showing upgrade dialog");
    checkBoxEnableTls->setChecked(false);

    if (showDialog) {
      UpgradeDialog dialog(parent);
      dialog.showDialog(
          QString("TLS Encryption"),
          QString("Please upgrade to %1 to enable TLS encryption.").arg(synergy::gui::kProProductName),
          synergy::gui::kUrlPersonalUpgrade
      );
    }
  }
}

void LicenseHandler::checkInvertConnectionCheckBox(
    QDialog *parent, QCheckBox *checkBoxInvertConnection, bool showDialog
) const
{
  if (!m_license.isInvertConnectionAvailable() && checkBoxInvertConnection->isChecked()) {
    qDebug("invert connection not available, showing upgrade dialog");
    checkBoxInvertConnection->setChecked(false);

    if (showDialog) {
      UpgradeDialog dialog(parent);
      dialog.showDialog(
          QString("Invert Connection"),
          QString("Please upgrade to %1 to enable the invert connection feature.")
              .arg(synergy::gui::kBusinessProductName),
          synergy::gui::kUrlContact
      );
    }
  }
}

void LicenseHandler::checkSettingsScopeRadioButton(
    QDialog *parent, QRadioButton *systemScope, QRadioButton *userScope, bool showDialog
) const
{
  if (!m_license.isSettingsScopeAvailable() && systemScope->isChecked()) {
    qDebug("settings scope not available, showing upgrade dialog");
    userScope->setChecked(true);

    if (showDialog) {
      UpgradeDialog dialog(parent);
      dialog.showDialog(
          QString("Settings Scope"),
          QString("Please upgrade to %1 to enable the settings scope feature.").arg(synergy::gui::kBusinessProductName),
          synergy::gui::kUrlContact
      );
    }
  }
}

const synergy::license::License &LicenseHandler::license() const
{
  return m_license;
}

Product::Edition LicenseHandler::productEdition() const
{
  return m_license.productEdition();
}

QString LicenseHandler::productName() const
{
  return QString::fromStdString(m_license.productName());
}

/// @param allowExpired If true, allow expired licenses to be set.
///     Useful for passing an expired license to the activation dialog.
LicenseHandler::SetSerialKeyResult LicenseHandler::setLicense(const QString &hexString, bool allowExpired)
{
  using enum LicenseHandler::SetSerialKeyResult;

  if (hexString.isEmpty()) {
    qFatal("serial key is empty");
  }

  qDebug() << "changing serial key to:" << hexString;
  auto serialKey = parseSerialKey(hexString);

  if (!serialKey.isValid) {
    qWarning() << "invalid serial key, ignoring:" << hexString;
    return kInvalid;
  }

  auto license = License(serialKey);
  if (m_time.hasTestTime()) {
    license.setNowFunc([this]() { return m_time.now(); });
  }

  if (!allowExpired && license.isExpired()) {
    qDebug("license is expired, ignoring");
    return kExpired;
  }

  const auto oldSerialKey = m_license.serialKey();
  m_license = license;

  // This delayed check logic seems really complex. Is it really worth the maintenance and testing cost?
  // Condition must run *after* the license member is set, since it's async callback uses this member.
  if (!m_license.isExpired() && m_license.isTimeLimited()) {
    auto secondsLeft = m_license.secondsLeft();
    if (secondsLeft.count() < INT_MAX) {
      const auto validateAt = secondsLeft + seconds{1};
      const auto interval = duration_cast<milliseconds>(validateAt);
      QTimer::singleShot(interval, this, &LicenseHandler::validate);
    } else {
      qDebug("license expiry too distant to schedule timer");
    }
  }

  if (serialKey == oldSerialKey) {
    qDebug("serial key did not change, ignoring");
    return kUnchanged;
  }

  // Reset activation when the serial key changes so new key can be activated on key change.
  m_settings.setActivated(false);
  m_settings.save();

  return kSuccess;
}

void LicenseHandler::validate()
{
  if (!m_license.isValid()) {
    qDebug("license validation failed, license invalid");
    showActivationDialog();
    return;
  }

  if (m_license.isExpired()) {
    qDebug("license validation failed, license expired");
    showActivationDialog();
    return;
  }

  qDebug("license validation succeeded");
}

void LicenseHandler::clampFeatures(bool enableTlsIfAvailable)
{
  if (enableTlsIfAvailable && m_license.isTlsAvailable()) {
    qDebug("tls available, enabling tls");
    m_appConfig->setTlsEnabled(true);
  } else if (m_appConfig->tlsEnabled() && !m_license.isTlsAvailable()) {
    qWarning("tls not available, disabling tls");
    m_appConfig->setTlsEnabled(false);
  }

  if (m_appConfig->invertConnection() && !m_license.isInvertConnectionAvailable()) {
    qWarning("invert connection not available, disabling invert connection");
    m_appConfig->setInvertConnection(false);
  }

  if (m_appConfig->isActiveScopeSystem() && !m_license.isSettingsScopeAvailable()) {
    qWarning("settings scope not available, disabling system scope");
    m_appConfig->setLoadFromSystemScope(false);
  }

  qDebug("committing default feature settings");
  m_appConfig->commit();
}

void LicenseHandler::disable()
{
  qDebug("disabling license handler");
  m_enabled = false;
}
