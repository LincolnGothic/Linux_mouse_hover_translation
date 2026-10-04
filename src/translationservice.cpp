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

TranslationService::TranslationService(QObject *parent) : QObject(parent)
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
        translate(request, text, source, target, settings.instance);
        return;
    }
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
