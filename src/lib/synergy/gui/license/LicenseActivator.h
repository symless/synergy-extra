/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2022-2025 Synergy Ltd.
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

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

class QNetworkReply;

namespace synergy::gui::license {

class LicenseActivator : public QObject
{
  Q_OBJECT

public:
  struct Data
  {
    QString machineSignature;
    QString hostnameSignature;
    QString serialKey;
    QString appVersion;
    QString osName;
    bool isServer;
  };

  explicit LicenseActivator();

  void activate(Data activateData);

  bool isBusy()
  {
    return m_isBusy;
  }

signals:
  void activationFailed(const QString &message);
  void activationSucceeded();

private slots:
  void handleResponse(QNetworkReply *reply);

private:
  QByteArray getRequestData(Data activateData);

  QNetworkAccessManager m_manager;
  bool m_isBusy = false;
};

} // namespace synergy::gui::license
