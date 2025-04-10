/*
 * Synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2016 Symless Ltd.
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

#include "ActivationDialog.h"

#include "CancelActivationDialog.h"
#include "gui/config/AppConfig.h"
#include "gui/styles.h"
#include "synergy/gui/constants.h"
#include "synergy/gui/license/LicenseHandler.h"
#include "synergy/gui/license/license_notices.h"
#include "synergy/license/parse_serial_key.h"
#include "ui_ActivationDialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QScreen>
#include <QStyle>
#include <QThread>

using namespace deskflow::gui;
using namespace synergy::gui;
using namespace synergy::license;

const QString successTitle = "Serial key";
const QString problemTitle = "Serial key problem";

ActivationDialog::ActivationDialog(QWidget *parent, AppConfig &appConfig, LicenseHandler &licenseHandler)
    : QDialog(parent),
      m_ui(new Ui::ActivationDialog),
      m_pAppConfig(&appConfig),
      m_licenseHandler(licenseHandler)
{
  m_ui->setupUi(this);

  m_ui->m_pLabelNotice->setStyleSheet(kStyleNoticeLabel);

  refreshSerialKey();

  if (!m_licenseHandler.license().isExpired()) {
    m_ui->m_widgetNotice->hide();
  }
}

void ActivationDialog::refreshSerialKey()
{
  const QString envSerialKey = qEnvironmentVariable("SYNERGY_TEST_SERIAL_KEY");
  if (!envSerialKey.isEmpty()) {
    qDebug("using serial key from env var");
    m_ui->m_pTextEditSerialKey->setText(envSerialKey);
  } else {
    qDebug("using serial key from config");
    const auto hexString = m_licenseHandler.license().serialKey().hexString;
    m_ui->m_pTextEditSerialKey->setText(QString::fromStdString(hexString));
  }

  m_ui->m_pTextEditSerialKey->setFocus();
  m_ui->m_pTextEditSerialKey->moveCursor(QTextCursor::End);

  const auto &license = m_licenseHandler.license();
  if (license.isTimeLimited()) {
    m_ui->m_pLabelNotice->setText(licenseNotice(license));
  }
}

ActivationDialog::~ActivationDialog()
{
  delete m_ui;
}

void ActivationDialog::reject()
{
  // don't show the cancel confirmation dialog if they've already registered,
  // since it's not relevant to customers who are changing their serial key.
  const auto &license = m_licenseHandler.license();
  if (license.isValid() && !license.isExpired()) {
    QDialog::reject();
    return;
  }

  // the accept button should be labeled "Exit" on the cancel dialog.
  CancelActivationDialog cancelActivationDialog(this);
  if (cancelActivationDialog.exec() == QDialog::Accepted) {
    QApplication::exit();
  }
}

void ActivationDialog::accept()
{
  using Result = LicenseHandler::SetSerialKeyResult;
  auto serialKey = m_ui->m_pTextEditSerialKey->toPlainText();

  if (serialKey.isEmpty()) {
    QMessageBox::information(this, "Activation", "Please enter a serial key.");
    return;
  }

  const auto result = m_licenseHandler.setLicense(serialKey);
  if (result != Result::kSuccess) {
    showResultDialog(result);
    return;
  }

  m_serialKeyChanged = true;
  showSuccessDialog();
  QDialog::accept();
}

void ActivationDialog::showResultDialog(LicenseHandler::SetSerialKeyResult result)
{
  switch (result) {
    using enum LicenseHandler::SetSerialKeyResult;

  case kUnchanged:
    QMessageBox::information(
        this, successTitle,
        "Heads up, the serial key you entered is valid but was the same as last time. "
        "Perhaps you intended to enter a different serial key."
    );
    QDialog::accept();
    break;

  case kInvalid:
    QMessageBox::warning(
        this, problemTitle,
        QString(
            "Invalid serial key. "
            R"(Please <a href="%1" style="color: %2">contact us</a> for help.)"
        )
            .arg(kUrlContact)
            .arg(kColorSecondary)
    );
    break;

  case kExpired:
    QMessageBox::warning(
        this, problemTitle,
        QString(
            "Sorry, that serial key has expired. "
            R"(Please <a href="%1" style="color: %2">renew</a> your license.)"
        )
            .arg(kUrlPurchase)
            .arg(kColorSecondary)
    );
    break;

  default:
    qFatal("unexpected change serial key result: %d", static_cast<int>(result));
  }
}

void ActivationDialog::showSuccessDialog()
{
  const auto &license = m_licenseHandler.license();

  QString title = successTitle;
  QString message = tr("<p>Thanks for entering your serial key for %1.</p>").arg(m_licenseHandler.productName());

  const auto tlsAvailable = m_licenseHandler.license().isTlsAvailable();
  if (tlsAvailable && m_pAppConfig->tlsEnabled()) {
    message += "<p>To ensure that TLS encryption works correctly, "
               "please use same serial key on all of your computers.</p>";
  }

  if (license.isTimeLimited()) {
    auto daysLeft = license.daysLeft().count();
    if (license.isTrial()) {
      title = "Trial started";
      message += QString("Your trial will expire in %1 %2.").arg(daysLeft).arg((daysLeft == 1) ? "day" : "days");
    } else if (license.isSubscription()) {
      message += QString("Your license will expire in %1 %2.").arg(daysLeft).arg((daysLeft == 1) ? "day" : "days");
    }
  }

  QMessageBox::information(this, title, message);
}

void ActivationDialog::showErrorDialog(const QString &message)
{
  QString fullMessage = QString(
                            "<p>There was a problem with your serial key.</p>"
                            R"(<p>Please <a href="%1" style="color: %2">contact us</a> )"
                            "and provide the following information:</p>"
                            "%3"
  )
                            .arg(kUrlContact)
                            .arg(kColorSecondary)
                            .arg(message);
  QMessageBox::warning(this, problemTitle, fullMessage);
}
