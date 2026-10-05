// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>

QString offlineDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/hover-translate";
}

QString offlineAsset(const QString &name)
{
    const QString base = QCoreApplication::applicationDirPath();
    for (const auto &directory : {base + "/share/hover-translate", base + "/../share/hover-translate"}) {
        const QString file = QDir(directory).absoluteFilePath(name);
        if (QFileInfo::exists(file)) return file;
    }
    return {};
}

QString offlinePython(const HoverSettings &settings)
{
    if (!settings.pythonPath.isEmpty()) return settings.pythonPath;
    const QString managed = offlineDataDirectory() + "/argos-env/bin/python";
    return QFileInfo::exists(managed) ? managed : QStringLiteral("python3");
}

SettingsStore::SettingsStore(QString fileName)
    : m_fileName(fileName.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.ini"
          : std::move(fileName)) {}

bool SettingsStore::validInstance(const QString &instance)
{
    const QUrl url(instance);
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()
        || url.hasQuery() || url.hasFragment())
        return false;
    if (url.scheme() == "https")
        return true;
    // HTTP is allowed only for a local/self-hosted service, including tests.
    return url.scheme() == "http"
        && (url.host() == "localhost" || QHostAddress(url.host()).isLoopback());
}

bool SettingsStore::validate(const HoverSettings &settings, QString *error)
{
    QString message;
    if (settings.target != "en" && settings.target != "zh-CN")
        message = QStringLiteral("Choose English or Simplified Chinese.");
    else if (settings.provider != "offline" && settings.provider != "mozhi")
        message = QStringLiteral("Choose offline translation or Mozhi.");
    else if (settings.provider == "mozhi" && !validInstance(settings.instance))
        message = QStringLiteral("Enter an HTTPS Mozhi server URL without credentials, a query, or a fragment. HTTP is allowed only on localhost.");
    else if (settings.textMode != "word" && settings.textMode != "line" && settings.textMode != "sentence")
        message = QStringLiteral("Choose Word, Line or Sentence for hover text.");
    else if (settings.dwellMs < 100 || settings.dwellMs > 3000)
        message = QStringLiteral("The hover delay must be between 100 and 3000 milliseconds.");
    if (error) *error = message;
    return message.isEmpty();
}

HoverSettings SettingsStore::load() const
{
    const QSettings file(m_fileName, QSettings::IniFormat);
    HoverSettings settings;
    settings.target = file.value("translation/target", settings.target).toString();
    settings.provider = file.value("translation/provider", settings.provider).toString();
    settings.instance = file.value("translation/instance", settings.instance).toString();
    settings.pythonPath = file.value("offline/python").toString();
    settings.packagesPath = file.value("offline/packages").toString();
    settings.dictionaryPath = file.value("offline/dictionary").toString();
    settings.useDictionary = file.value("offline/useDictionary", true).toBool();
    settings.textMode = file.value("hover/textMode", settings.textMode).toString();
    settings.dwellMs = file.value("hover/dwellMs", settings.dwellMs).toInt();
    settings.enabled = file.value("hover/enabled", settings.enabled).toBool();
    settings.tessdataPath = file.value("ocr/tessdataPath").toString();
    // Corrupt or hand-edited files must not enable capture with invalid settings.
    if (!validate(settings)) {
        settings.enabled = false;
        if (settings.provider != "offline" && settings.provider != "mozhi") settings.provider = "offline";
        if (settings.target != "en" && settings.target != "zh-CN")
            settings.target = "zh-CN";
        if (!validInstance(settings.instance))
            settings.instance = "https://mozhi.aryak.me";
        if (settings.textMode != "word" && settings.textMode != "line" && settings.textMode != "sentence") settings.textMode = "line";
        settings.dwellMs = qBound(100, settings.dwellMs, 3000);
    }
    return settings;
}

bool SettingsStore::save(const HoverSettings &settings, QString *error) const
{
    if (!validate(settings, error))
        return false;
    if (!QDir().mkpath(QFileInfo(m_fileName).absolutePath())) {
        if (error) *error = QStringLiteral("Could not create the settings directory.");
        return false;
    }
    QSettings file(m_fileName, QSettings::IniFormat);
    file.setValue("translation/target", settings.target);
    file.setValue("translation/provider", settings.provider);
    file.setValue("translation/instance", settings.instance);
    file.setValue("offline/python", settings.pythonPath);
    file.setValue("offline/packages", settings.packagesPath);
    file.setValue("offline/dictionary", settings.dictionaryPath);
    file.setValue("offline/useDictionary", settings.useDictionary);
    file.setValue("hover/textMode", settings.textMode);
    file.setValue("hover/dwellMs", settings.dwellMs);
    file.setValue("hover/enabled", settings.enabled);
    file.setValue("ocr/tessdataPath", settings.tessdataPath);
    file.sync();
    if (file.status() != QSettings::NoError) {
        if (error) *error = QStringLiteral("Could not save settings.");
        return false;
    }
    return true;
}
