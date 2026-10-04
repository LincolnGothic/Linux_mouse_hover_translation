// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "translationservice.h"
#include "settings.h"
#include <QUrl>

TranslationService::TranslationService(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        const auto request = m_request;
        cancel();
        emit failed(request, tr("The translation server did not respond in time."));
    });
}

void TranslationService::cancel()
{
    m_timeout.stop();
    if (!m_translator) return;
    // Each request owns its Crow translator. Aborted replies and state-machine
    // events from an old request can never complete a newer request.
    disconnect(m_translator, nullptr, this, nullptr);
    m_translator->abort();
    m_translator->deleteLater();
    m_translator = nullptr;
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
