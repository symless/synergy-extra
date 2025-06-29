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
#include "common/constants.h"
#include "dialogs/UpgradeDialog.h"
#include "gui/config/AppConfig.h"
#include "gui/core/CoreProcess.h"
#include "gui/styles.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/license/license_utils.h"
#include "synergy/license/Product.h"

#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QCryptographicHash>
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
using namespace synergy::gui;
using namespace deskflow::gui;
using License = synergy::license::License;

LicenseHandler::LicenseHandler()
{
  m_enabled = synergy::gui::license::isActivationEnabled();

  connect(&m_activator, &LicenseActivator::activationFailed, this, [this](const QString &message) {
    QString fullMessage = QString(
                              "<p>There was a problem activating your license.</p>"
                              "%3"
                              R"(<p>Please <a href="%1" style="color: %2">contact us</a> )"
                              "if there is anything we can do to help.</p>"
    )
                              .arg(kUrlContact)
                              .arg(kColorSecondary)
                              .arg(message);
    QMessageBox::warning(m_pMainWindow, "Activation failed", fullMessage);

    qWarning("activation failed, showing serial key dialog");
    return showSerialKeyDialog();
  });

  connect(&m_activator, &LicenseActivator::activationSucceeded, this, [this] {
    qDebug("license activation succeeded, saving settings");
    m_settings.setActivated(true);
    m_settings.sync();

    if (m_pCoreProcess == nullptr) {
      qFatal("core process not set");
    }

    qDebug("resuming core process after activation");
    m_pCoreProcess->start();
  });
}

void LicenseHandler::handleMainWindow(
    QMainWindow *mainWindow, AppConfig *appConfig, deskflow::gui::CoreProcess *coreProcess
)
{
  // Must still be set as these are used when not enabled.
  m_pMainWindow = mainWindow;
  m_pAppConfig = appConfig;
  m_pCoreProcess = coreProcess;

  if (!m_enabled) {
    qDebug("license handler disabled, skipping main window handler");
    return;
  }

  qDebug("main window create handled");

  if (!loadSettings()) {
    qFatal("failed to load license settings");
  }
}

bool LicenseHandler::handleAppStart()
{
  if (m_pMainWindow == nullptr) {
    qFatal("main window not set");
  }

  if (m_pAppConfig == nullptr) {
    qFatal("app config not set");
  }

  if (!m_enabled) {
    qDebug("license handler disabled, skipping start handler");
    m_pMainWindow->setWindowTitle(SYNERGY_PRODUCT_NAME);
    return true;
  }

  updateWindowTitle();

  const auto serialKeyAction = new QAction("Change serial key", m_pMainWindow);
  QObject::connect(serialKeyAction, &QAction::triggered, [this] { showSerialKeyDialog(); });

  const auto licenseMenu = new QMenu("License");
  licenseMenu->addAction(serialKeyAction);
  m_pMainWindow->menuBar()->addAction(licenseMenu->menuAction());

  const auto checkResult = check();
  if (!checkResult) {
    return false;
  }

  qDebug("license is valid, continuing with start");
  updateWindowTitle();
  clampFeatures();
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
    qDebug("license handler, tls checkbox toggled");
    checkTlsCheckBox(parent, enableTls, true);
  };
  QObject::connect(enableTls, &QCheckBox::toggled, onTlsToggle);

  const auto onInvertConnectionToggle = [this, parent, invertConnection] {
    qDebug("license handler, invert connection checkbox toggled");
    checkInvertConnectionCheckBox(parent, invertConnection, true);
  };
  QObject::connect(invertConnection, &QCheckBox::toggled, onInvertConnectionToggle);

  checkTlsCheckBox(parent, enableTls, false);
  checkInvertConnectionCheckBox(parent, invertConnection, false);
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

bool LicenseHandler::handleCoreStart()
{
  // HACK: For some reason, the core start trigger gets called twice when clicking the 'start' button.
  // If the activator is called twice in quick succession, the core is started twice.
  if (m_activator.isBusy()) {
    qDebug("activator is busy, skipping core start handler");
    return false;
  }

  if (!m_enabled) {
    qDebug("license handler disabled, skipping core start handler");
    return true;
  }

  if (m_pAppConfig == nullptr) {
    qFatal("app config not set");
  }

  if (m_pMainWindow == nullptr) {
    qFatal("main window not set");
  }

  if (m_pCoreProcess == nullptr) {
    qFatal("core process not set");
  }

  if (m_settings.activated()) {
    qDebug("license is activated, starting core");
    return true;
  }

  if (m_license.serialKey().isOffline) {
    qDebug("offline serial key, starting core");
    return true;
  }

  qInfo("activating license");

  const auto machineId = QSysInfo::machineUniqueId();
  const auto hostname = QHostInfo::localHostName();

  // Protect the customer by anonymizing the machine ID and hostname so that if leaked accidentally,
  // the information cannot be used as an attack vector on the customer by a bad actor.
  const QString machineSignature = QCryptographicHash::hash(machineId, QCryptographicHash::Sha256).toHex();
  const auto hostnameSignature = QCryptographicHash::hash(hostname.toUtf8(), QCryptographicHash::Sha256).toHex();

  const auto serialKey = QString::fromStdString(m_license.serialKey().hexString);
  const auto osName = QSysInfo::prettyProductName();
  const auto appVersion = kVersion;
  const auto isServer = m_pAppConfig->serverGroupChecked();

  m_activator.activate({machineSignature, hostnameSignature, serialKey, appVersion, osName, isServer});

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
      return showSerialKeyDialog();
    }
  }

  return true;
}

void LicenseHandler::saveSettings()
{
  const auto hexString = m_license.serialKey().hexString;
  m_settings.setSerialKey(QString::fromStdString(hexString));
  m_settings.sync();
}

bool LicenseHandler::showSerialKeyDialog()
{
  if (!m_settings.isWritable()) {
    QMessageBox::warning(
        m_pMainWindow, "Write access required",
        tr("<p>The settings file is not writable:</p>"
           "<p><code>%1</code></p>"
           "<p>Please check the file permissions and try again.</p>")
            .arg(m_settings.fileName())
    );
    return false;
  }

  ActivationDialog dialog(m_pMainWindow, *m_pAppConfig, *this);
  const auto result = dialog.exec();
  if (result != QDialog::Accepted) {
    qWarning("license serial key dialog declined");
    return false;
  }

  if (dialog.serialKeyChanged()) {
    // Reset activation so new serial key can be activated.
    qDebug("serial key changed, updating settings");
    m_settings.setActivated(false);
    m_settings.sync();
  }

  saveSettings();
  updateWindowTitle();
  clampFeatures();

  if (dialog.serialKeyChanged() && m_pCoreProcess->isStarted()) {
    qDebug("restarting core on serial key change");
    m_pCoreProcess->restart();
  }

  qDebug("license serial key dialog accepted");
  return true;
}

void LicenseHandler::updateWindowTitle() const
{
  const auto productName = QString::fromStdString(m_license.productName());
  qDebug("updating main window title: %s", qPrintable(productName));
  m_pMainWindow->setWindowTitle(productName);
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
      QTimer::singleShot(interval, this, &LicenseHandler::check);
    } else {
      qDebug("license expiry too distant to schedule timer");
    }
  }

  if (serialKey == oldSerialKey) {
    qDebug("serial key did not change, ignoring");
    return kUnchanged;
  }

  return kSuccess;
}

bool LicenseHandler::check()
{
  if (!m_license.isValid()) {
    qDebug("license validation failed, license invalid");
    return showSerialKeyDialog();
  } else if (m_license.isExpired()) {
    qDebug("license validation failed, license expired");
    return showSerialKeyDialog();
  } else if (m_license.isExpiringSoon()) {
    qDebug("license is expiring soon, skipping activation dialog");
    showSerialKeyDialog();
    // Return true even if dialog cancelled, since expiring soon licenses are still valid.
    return true;
  } else {
    qDebug("license validation succeeded");
    return true;
  }
}

void LicenseHandler::clampFeatures()
{
  if (m_pAppConfig->tlsEnabled() && !m_license.isTlsAvailable()) {
    qWarning("tls not available, disabling tls");
    m_pAppConfig->setTlsEnabled(false);
  }

  if (m_pAppConfig->invertConnection() && !m_license.isInvertConnectionAvailable()) {
    qWarning("invert connection not available, disabling invert connection");
    m_pAppConfig->setInvertConnection(false);
  }

  if (m_pAppConfig->isSystemScope() && !m_license.isSettingsScopeAvailable()) {
    qFatal("settings scope not available");
  }

  qDebug("committing default feature settings");
  m_pAppConfig->commit();
}

void LicenseHandler::disable()
{
  qDebug("disabling license handler");
  m_enabled = false;
}
