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

#include "LicenseActivator.h"

#include "synergy/gui/constants.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSysInfo>
#include <QTimer>

namespace synergy::gui::license {

QString activateUrl()
{
  const auto envVar = qEnvironmentVariable("SYNERGY_TEST_API_URL_ACTIVATE");
  return envVar.isEmpty() ? kUrlApiLicenseActivate : envVar;
}

LicenseActivator::LicenseActivator()
{
  connect(&m_manager, &QNetworkAccessManager::finished, this, &LicenseActivator::handleResponse);
}

void LicenseActivator::activate(Data activateData)
{
  const auto url = QUrl(activateUrl());
  qDebug().noquote() << "activating with url:" << url.toString();

  auto request = QNetworkRequest(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  m_manager.post(request, getRequestData(activateData));
}

void LicenseActivator::handleResponse(QNetworkReply *reply)
{
  if (!reply) {
    qWarning("no activation reply");
    Q_EMIT activationFailed("License activation failed, empty network reply.");
    return;
  }

  const auto response = reply->readAll();

  if (reply->error() != QNetworkReply::NoError) {
    const auto kLimit = 200;
    const auto responseSliced = response.length() > kLimit ? response.sliced(0, kLimit) + "..." : response;
    qWarning().noquote() << "activation error:" << reply->error() << reply->errorString() << responseSliced;
    Q_EMIT activationFailed("License activation failed, there was a network error.");
    reply->deleteLater();
    return;
  }

  qDebug().noquote() << "activation response:" << response;
  const auto jsonDoc = QJsonDocument::fromJson(response);
  if (response.isNull()) {
    qWarning("empty activation response");
    Q_EMIT activationFailed("License activation failed, the server sent an empty response.");
    reply->deleteLater();
    return;
  }

  const auto json = jsonDoc.object();
  if (json["status"].toString() != "success") {
    const auto status = json["status"].toString();
    const auto message = json["message"].toString();

    if (!status.isEmpty()) {
      qWarning().noquote() << "activation status:" << status;
    } else {
      qWarning("activation status was empty");
    }

    if (!message.isEmpty()) {
      qWarning().noquote() << "activation message:" << message;
      Q_EMIT activationFailed(message);
    } else {
      qWarning("activation message was empty");
      Q_EMIT activationFailed("License activation failed, unknown error.");
    }

    reply->deleteLater();
    return;
  }

  qDebug().noquote() << "activation succeeded";
  Q_EMIT activationSucceeded();
  reply->deleteLater();
}

QByteArray LicenseActivator::getRequestData(Data activateData)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  QString uuid(QSysInfo::machineUniqueId());
#else
  QString uuid;
#endif

  if (uuid.isEmpty()) {
    qFatal("failed to get machine unique id");
  }

  QJsonObject requestData;
  requestData["uuid"] = uuid;
  requestData["uuidType"] = "System";
  requestData["serialKey"] = activateData.serialKey;
  requestData["isServer"] = activateData.isServer;

  return QJsonDocument(requestData).toJson();
}

}; // namespace synergy::gui::license
