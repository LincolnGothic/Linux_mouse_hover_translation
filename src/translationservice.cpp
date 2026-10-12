// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "translationservice.h"
#include "settings.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QJsonArray>
#include <QUrlQuery>
#include <memory>

TranslationService::TranslationService(QObject *parent, QNetworkAccessManager *network, CloudStore cloud)
    : QObject(parent), m_network(network ? network : new QNetworkAccessManager(this)),
      m_cloud(std::move(cloud))
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        const auto request = m_request;
        cancel();
        stopWorker();
        emit failed(request, tr("Translation took too long. Try a smaller region or shorter text."));
    });
    connect(&m_worker, &QProcess::readyReadStandardOutput, this, &TranslationService::readOfflineOutput);
    connect(&m_worker, &QProcess::readyReadStandardError, this, [this] { m_worker.readAllStandardError(); });
    connect(&m_worker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_active) return;
        const auto request = m_request;
        cancel();
        emit failed(request, tr("Could not start offline translation. Install the English / Chinese models in Settings."));
    });
    connect(&m_worker, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this] {
        m_ready = false; m_busy = false; m_workerKey.clear();
        if (!m_active) return;
        const auto request = m_request;
        cancel();
        emit failed(request, tr("Offline translation stopped. Install or repair the offline models in Settings."));
    });
}

TranslationService::~TranslationService() { cancel(); stopWorker(); }

void TranslationService::stopWorker()
{
    m_active = false;
    m_ready = false; m_busy = false; m_workerKey.clear(); m_output.clear();
    if (m_worker.state() != QProcess::NotRunning) {
        m_worker.kill();
        m_worker.waitForFinished(1000);
    }
}

void TranslationService::cancel()
{
    m_timeout.stop();
    m_active = false;
    m_pending.clear();
    ++m_serial;
    if (m_azureReply) {
        auto *reply = m_azureReply.data();
        m_azureReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    if (!m_translator) return;
    // Each request owns its Crow translator. Aborted replies and state-machine
    // events from an old request can never complete a newer request.
    disconnect(m_translator, nullptr, this, nullptr);
    m_translator->abort();
    m_translator->deleteLater();
    m_translator = nullptr;
}

void TranslationService::translate(quint64 request, const QString &text, const QString &source,
                                   const QString &target, const HoverSettings &settings)
{
    if (settings.provider == "mozhi") {
        stopWorker();
        translateMozhi(request, text, source, target, settings.instance);
        return;
    }
    if (settings.provider == "azure") {
        stopWorker();
        translateAzure(request, text, source, target);
        return;
    }
    if (settings.provider == "offline") {
        translateOffline(request, text, source, target, settings);
        return;
    }
    cancel();
    emit failed(request, tr("Choose Offline, Azure or Mozhi translation."));
}

void TranslationService::translateOffline(quint64 request, const QString &text, const QString &source,
                                          const QString &target, const HoverSettings &settings)
{
    cancel();
    m_request = request;
    if (settings.provider != "offline" || text.trimmed().isEmpty() || text.size() > 10000
        || (source != "en" && source != "zh-CN") || (target != "en" && target != "zh-CN")) {
        emit failed(request, tr("Use English or Simplified Chinese text, up to 10,000 characters."));
        return;
    }
    const QString script = offlineAsset("argos_bridge.py");
    if (script.isEmpty()) { emit failed(request, tr("The offline helper is missing. Reinstall Hover Translate.")); return; }
    const QString packages = settings.packagesPath.isEmpty()
        ? offlineDataDirectory() + "/argos-packages" : settings.packagesPath;
    const QString dictionary = settings.useDictionary ? (settings.dictionaryPath.isEmpty()
        ? offlineDataDirectory() + "/cedict.u8" : settings.dictionaryPath) : QString();
    const QString python = offlinePython(settings);
    const QString key = python + '\n' + packages + '\n' + dictionary + '\n'
        + QString::number(QFileInfo(dictionary).lastModified().toMSecsSinceEpoch()) + '\n'
        + QString::number(QFileInfo(packages + "/download-manifest.json").lastModified().toMSecsSinceEpoch());
    if (key != m_workerKey) stopWorker();
    m_active = true;
    m_pending = QJsonDocument(QJsonObject{{"id", QString::number(m_serial)}, {"text", text},
        {"source", source}, {"target", target}}).toJson(QJsonDocument::Compact) + '\n';
    m_timeout.start(m_offlineTimeoutMs);
    if (m_worker.state() == QProcess::NotRunning) {
        m_workerKey = key;
        m_ready = false; m_busy = false; m_output.clear();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PYTHONUNBUFFERED", "1");
        m_worker.setProcessEnvironment(environment);
        m_worker.start(python, {script, "--worker", "--packages-dir", packages, "--dictionary", dictionary});
    }
    sendOfflineRequest();
}

void TranslationService::sendOfflineRequest()
{
    if (!m_active || !m_ready || m_busy || m_pending.isEmpty()) return;
    m_busy = true;
    m_worker.write(m_pending);
    m_pending.clear();
}

void TranslationService::readOfflineOutput()
{
    m_output += m_worker.readAllStandardOutput();
    if (m_output.size() > 1024 * 1024) {
        const auto request = m_request;
        cancel(); stopWorker();
        emit failed(request, tr("The offline helper returned an invalid response."));
        return;
    }
    while (m_output.contains('\n')) {
        const int end = m_output.indexOf('\n');
        const auto line = m_output.left(end); m_output.remove(0, end + 1);
        const auto object = QJsonDocument::fromJson(line).object();
        if (object.value("ready").toBool()) { m_ready = true; sendOfflineRequest(); continue; }
        if (!object.contains("id")) continue;
        m_busy = false;
        if (m_active && object.value("id").toString() == QString::number(m_serial)) {
            const auto request = m_request;
            const QString result = object.value("translation").toString();
            const QString error = object.value("error").toString();
            cancel();
            if (!error.isEmpty() || result.trimmed().isEmpty())
                emit failed(request, error.isEmpty() ? tr("No offline translation was returned.") : error);
            else emit translated(request, result);
        }
        sendOfflineRequest();
    }
}

void TranslationService::translate(quint64 request, const QString &text, const QString &source,
                                   const QString &target, const QString &instance)
{
    stopWorker();
    translateMozhi(request, text, source, target, instance);
}

void TranslationService::translateMozhi(quint64 request, const QString &text, const QString &source,
                                        const QString &target, const QString &instance)
{
    cancel();
    m_request = request;
    if (!SettingsStore::validInstance(instance) || text.trimmed().isEmpty()
        || (source != "en" && source != "zh-CN") || (target != "en" && target != "zh-CN")) {
        emit failed(request, tr("Check the translation server and language settings."));
        return;
    }
    auto *translator = new OnlineTranslator(this);
    m_translator = translator;
    translator->setInstance(QUrl(instance).toString(QUrl::StripTrailingSlash));
    connect(translator, &OnlineTranslator::requestSubmitted, this, [this](qint64 characters) {
        QString error;
        if (!m_cloud.record(characters, &error)) {
            const auto request = m_request;
            cancel();
            emit failed(request, error);
        }
    });
    connect(translator, &OnlineTranslator::finished, this, [this, translator, request] {
        if (m_translator != translator) return;
        m_timeout.stop();
        const bool success = translator->error() == OnlineTranslator::NoError
            && !translator->translation().trimmed().isEmpty();
        const QString result = translator->translation().trimmed();
        cancel();
        if (success) emit translated(request, result);
        else emit failed(request, tr("Translation unavailable. Check the server address and its Google engine."));
    });
    m_timeout.start(m_timeoutMs);
    translator->translate(text, OnlineTranslator::Google,
        target == "en" ? OnlineTranslator::English : OnlineTranslator::ChineseSimplified,
        source == "en" ? OnlineTranslator::English : OnlineTranslator::ChineseSimplified);
}

void TranslationService::translateAzure(quint64 request, const QString &text, const QString &source,
                                        const QString &target)
{
    cancel(); m_request = request;
    const auto from = azureLanguage(source), to = azureLanguage(target);
    if (from.isEmpty() || to.isEmpty()) {
        emit failed(request, tr("Azure does not support this language combination. Choose English or Simplified Chinese.")); return;
    }
    if (text.trimmed().isEmpty() || text.size() > 10000) {
        emit failed(request, tr("Use non-empty text, up to 10,000 characters.")); return;
    }
    if (source == target) { emit translated(request, text); return; }
    QString error;
    const auto credentials = m_cloud.credentials(&error);
    if (!error.isEmpty()) { emit failed(request, error); return; }
    const auto key = credentials.value("key").toString();
    if (key.isEmpty()) {
        emit failed(request, tr("Configure your own Azure credentials in Settings.")); return;
    }
    QUrl url("https://api.cognitive.microsofttranslator.com/translate");
    QUrlQuery query; query.addQueryItem("api-version", "3.0");
    query.addQueryItem("from", from); query.addQueryItem("to", to); url.setQuery(query);
    QNetworkRequest http(url);
    http.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    http.setRawHeader("Ocp-Apim-Subscription-Key", key.toUtf8());
    const auto region = credentials.value("region").toString();
    if (!region.isEmpty()) http.setRawHeader("Ocp-Apim-Subscription-Region", region.toUtf8());
    if (!m_cloud.record(text.toUcs4().size(), &error)) { emit failed(request, error); return; }
    const auto body = QJsonDocument(QJsonArray{QJsonObject{{"Text", text}}}).toJson(QJsonDocument::Compact);
    auto *reply = m_network->post(http, body);
    m_azureReply = reply;
    const auto serial = m_serial;
    const auto received = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, serial, request, received] {
        if (m_azureReply != reply || m_serial != serial) return;
        *received += reply->readAll();
        if (received->size() > 2000000) {
            cancel(); emit failed(request, tr("Azure returned an invalid response. Retry later."));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial, request, received, to] {
        if (m_azureReply != reply || m_serial != serial) return;
        *received += reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error, result;
        if (status && status != 200) error = azureHttpError(status);
        else if (reply->error() != QNetworkReply::NoError || !status)
            error = tr("Azure network request failed. Check your connection and retry.");
        else result = parseAzureResponse(*received, to, &error);
        cancel();
        if (!error.isEmpty()) emit failed(request, error);
        else emit translated(request, result);
    });
    m_timeout.start(m_timeoutMs);
}
