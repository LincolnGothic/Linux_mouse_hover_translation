// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDate>
#include <QJsonObject>
#include <QString>

QString azureLanguage(const QString &language);
QString azureHttpError(int status);
QString parseAzureResponse(const QByteArray &body, const QString &target, QString *error);

// Separate from INI settings. Windows credentials use Credential Manager; Linux uses owner-only files.
// Credentials and usage never contain translation text.
class CloudStore {
public:
    explicit CloudStore(QString directory = {});
    QJsonObject credentials(QString *error = nullptr) const;
    bool saveCredentials(const QString &key, const QString &region, QString *error = nullptr) const;
    QJsonObject usage(QDate today = QDate::currentDate(), QString *error = nullptr) const;
    bool record(qint64 characters, QString *error = nullptr, QDate today = QDate::currentDate()) const;
    QString usageLabel() const;
private:
    bool privateDirectory(QString *error) const;
    QString m_directory;
};
