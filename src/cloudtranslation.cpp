// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloudtranslation.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include "settings.h"
#include <QCryptographicHash>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
void problem(QString *error, const QString &message) { if (error) *error = message; }
#ifdef Q_OS_WIN
QString credentialTarget(const QString &directory)
{
    const auto path = QDir::cleanPath(QFileInfo(directory).absoluteFilePath()).toCaseFolded();
    return "HoverTranslate/Azure/" + QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
}
#else
bool ownedPrivate(const QString &path, bool directory)
{
    struct stat info {};
    if (::lstat(QFile::encodeName(path).constData(), &info) != 0) return false;
    return info.st_uid == ::getuid() && !(info.st_mode & 0077)
        && (directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode));
}
#endif
bool writePrivate(const QString &path, const QJsonObject &value)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return false;
#ifndef Q_OS_WIN
    if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) return false;
#endif
    const auto data = QJsonDocument(value).toJson(QJsonDocument::Compact);
    return file.write(data) == data.size() && file.commit();
}
}

QString azureLanguage(const QString &language)
{
    if (language == "en") return "en";
    if (language == "zh-CN") return "zh-Hans";
    return {};
}

QString azureHttpError(int status)
{
    switch (status) {
    case 400: return QStringLiteral("Azure rejected the language combination or text. Check source and target languages.");
    case 401: return QStringLiteral("Azure authentication failed. Check your credentials and resource region.");
    case 403: return QStringLiteral("Azure access denied. Check credentials, region and resource quota.");
    case 408: return QStringLiteral("Azure translation timed out. Try shorter text or retry later.");
    case 429: return QStringLiteral("Azure quota or rate limit reached. Check your quota and retry later.");
    default: return status >= 300 && status < 400
        ? QStringLiteral("Azure redirect refused. Check the Translator endpoint.")
        : QStringLiteral("Azure is unavailable. Retry later and check the service status.");
    }
}

QString parseAzureResponse(const QByteArray &body, const QString &target, QString *error)
{
    problem(error, {});
    const QString invalid = QStringLiteral("Azure returned an invalid response. Retry later.");
    const QString empty = QStringLiteral("Azure returned an empty translation. Try different text.");
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(body, &parse);
    if (body.size() > 2000000 || parse.error != QJsonParseError::NoError || !document.isArray()) {
        problem(error, invalid); return {};
    }
    const auto results = document.array();
    if (results.isEmpty()) { problem(error, empty); return {}; }
    if (results.size() != 1 || !results[0].isObject()
        || !results[0].toObject().value("translations").isArray()) {
        problem(error, invalid); return {};
    }
    const auto translations = results[0].toObject().value("translations").toArray();
    if (translations.isEmpty()) { problem(error, empty); return {}; }
    if (translations.size() != 1 || !translations[0].isObject()
        || !translations[0].toObject().value("text").isString()
        || translations[0].toObject().value("to").toString(target) != target) {
        problem(error, invalid); return {};
    }
    const auto text = translations[0].toObject().value("text").toString().trimmed();
    if (text.isEmpty()) problem(error, empty);
    return text;
}

CloudStore::CloudStore(QString directory) : m_directory(directory.isEmpty()
#ifdef Q_OS_WIN
    ? offlineDataDirectory() + "/cloud"
#else
    ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/hover-translate/cloud"
#endif
    : std::move(directory)) {}

bool CloudStore::privateDirectory(QString *error) const
{
    problem(error, {});
#ifdef Q_OS_WIN
    if (!QDir().mkpath(m_directory) || !QFileInfo(m_directory).isDir()
        || QFileInfo(m_directory).isSymLink()) {
        problem(error, QStringLiteral("Cannot create the cloud directory; symbolic links are refused."));
        return false;
    }
#else
    if (!QFileInfo::exists(m_directory)) {
        if (!QDir().mkpath(QFileInfo(m_directory).absolutePath())
            || ::mkdir(QFile::encodeName(m_directory).constData(), 0700) != 0) {
            problem(error, QStringLiteral("Cannot create the private cloud directory.")); return false;
        }
    }
    if (!ownedPrivate(m_directory, true)) {
        problem(error, QStringLiteral("Cloud directory needs owner-only permissions (chmod 700); symbolic links are refused."));
        return false;
    }
#endif
    return true;
}

QJsonObject CloudStore::credentials(QString *error) const
{
    problem(error, {});
#ifdef Q_OS_WIN
    const auto target = credentialTarget(m_directory).toStdWString();
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() != ERROR_NOT_FOUND)
            problem(error, QStringLiteral("Cannot read Azure credentials from Windows Credential Manager."));
        return {};
    }
    const QByteArray data(reinterpret_cast<const char *>(credential->CredentialBlob),
                          int(credential->CredentialBlobSize));
    CredFree(credential);
#else
    if (!QFileInfo::exists(m_directory)) return {};
    if (!privateDirectory(error)) return {};
    const auto path = m_directory + "/azure-credentials.json";
    if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) return {};
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { problem(error, QStringLiteral("Cannot read private Azure credentials; symbolic links are refused.")); return {}; }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd); problem(error, QStringLiteral("Cannot read private Azure credentials.")); return {};
    }
    struct stat info {};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)
        || info.st_uid != ::getuid() || (info.st_mode & 0077)) {
        problem(error, QStringLiteral("Azure credentials need owner-only permissions (chmod 600).")); return {};
    }
    const auto data = file.read(8192);
#endif
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(data, &parse);
    const auto value = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || !value.value("key").isString()
        || value.value("key").toString().trimmed().isEmpty()
        || value.value("key").toString().size() > 2048
        || !value.value("region").isString()
        || !QRegularExpression("^[a-z0-9-]*$").match(value.value("region").toString()).hasMatch()
        || value.value("key").toString().contains(QRegularExpression("[\\r\\n]"))) {
        problem(error, QStringLiteral("Invalid private Azure credentials. Configure them again in Settings.")); return {};
    }
    return value;
}

bool CloudStore::saveCredentials(const QString &key, const QString &region, QString *error) const
{
    if (key.trimmed().isEmpty() || key.size() > 2048 || key.contains(QRegularExpression("[\\r\\n]"))
        || !QRegularExpression("^[a-z0-9-]*$").match(region).hasMatch()) {
        problem(error, QStringLiteral("Enter an Azure resource key and region, for example westus.")); return false;
    }
    if (!privateDirectory(error)) return false;
#ifdef Q_OS_WIN
    auto data = QJsonDocument(QJsonObject{{"key", key.trimmed()}, {"region", region}})
                    .toJson(QJsonDocument::Compact);
    if (data.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        problem(error, QStringLiteral("Azure credentials exceed the Windows Credential Manager size limit."));
        return false;
    }
    auto target = credentialTarget(m_directory).toStdWString();
    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.CredentialBlobSize = DWORD(data.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(data.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0)) {
        problem(error, QStringLiteral("Cannot save Azure credentials in Windows Credential Manager."));
        return false;
    }
#else
    const auto path = m_directory + "/azure-credentials.json";
    if ((QFileInfo::exists(path) || QFileInfo(path).isSymLink()) && !ownedPrivate(path, false)) {
        problem(error, QStringLiteral("Azure credentials need owner-only permissions (chmod 600); symbolic links are refused.")); return false;
    }
    if (!writePrivate(path, {{"key", key.trimmed()}, {"region", region}})) {
        problem(error, QStringLiteral("Cannot save private Azure credentials. Check directory permissions.")); return false;
    }
#endif
    return true;
}

QJsonObject CloudStore::usage(QDate today, QString *error) const
{
    problem(error, {});
    const auto month = today.toString("yyyy-MM");
    QJsonObject value{{"month", month}, {"requests", 0}, {"characters", 0}};
    QFile file(m_directory + "/cloud-usage.json");
    if (!file.exists()) return value;
    if (!file.open(QIODevice::ReadOnly)) {
        problem(error, QStringLiteral("Cannot read cloud usage. Check cloud-usage.json.")); return {};
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        problem(error, QStringLiteral("Cannot read cloud usage. Check cloud-usage.json.")); return {};
    }
    const auto stored = document.object();
    if (stored.value("month").toString() != month) return value;
    if (!stored.value("requests").isDouble() || !stored.value("characters").isDouble()
        || stored.value("requests").toDouble() < 0 || stored.value("characters").toDouble() < 0) {
        problem(error, QStringLiteral("Invalid cloud usage counters. Check cloud-usage.json.")); return {};
    }
    value["requests"] = stored.value("requests"); value["characters"] = stored.value("characters");
    return value;
}

bool CloudStore::record(qint64 characters, QString *error, QDate today) const
{
    if (!privateDirectory(error)) return false;
    QLockFile lock(m_directory + "/cloud-usage.lock");
    if (!lock.tryLock(1000)) { problem(error, QStringLiteral("Cloud usage is busy. Retry shortly.")); return false; }
    QString readError;
    auto value = usage(today, &readError);
    if (!readError.isEmpty()) { problem(error, readError); return false; }
    value["requests"] = value.value("requests").toDouble() + 1;
    value["characters"] = value.value("characters").toDouble() + characters;
    if (!writePrivate(m_directory + "/cloud-usage.json", value)) {
        problem(error, QStringLiteral("Cannot save cloud usage. Check directory permissions.")); return false;
    }
    return true;
}

QString CloudStore::usageLabel() const
{
    QString error; const auto value = usage(QDate::currentDate(), &error);
    if (!error.isEmpty()) return error;
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    return QStringLiteral("This month: %1 requests · %2 characters")
        .arg(locale.toString(qint64(value.value("requests").toDouble())),
             locale.toString(qint64(value.value("characters").toDouble())));
}
