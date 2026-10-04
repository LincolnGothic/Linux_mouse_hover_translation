// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hoverpolicy.h"
#include <QChar>
#include <limits>

bool HoverPolicy::moved(QPoint first, QPoint second)
{
    const QPoint delta = first - second;
    return delta.x() * delta.x() + delta.y() * delta.y() > 16;
}

void HoverPolicy::reset()
{
    ++m_generation;
    m_initialized = false;
    m_attempted = false;
}

HoverPolicy::Action HoverPolicy::update(QPoint position, bool blocked, qint64 nowMs)
{
    if (!m_initialized || blocked != m_blocked || moved(position, m_anchor)) {
        m_initialized = true;
        m_anchor = position;
        m_since = nowMs;
        m_blocked = blocked;
        m_attempted = false;
        ++m_generation;
        return Invalidated;
    }
    if (blocked || m_attempted || nowMs - m_since < m_dwellMs)
        return Idle;
    m_attempted = true;
    return Capture;
}

QString HoverPolicy::sourceLanguage(const QString &text)
{
    bool han = false, latin = false;
    for (const char32_t code : text.toUcs4()) {
        const auto script = QChar::script(code);
        if (script == QChar::Script_Hiragana || script == QChar::Script_Katakana
            || script == QChar::Script_Hangul)
            return {};
        han |= script == QChar::Script_Han;
        latin |= (code >= U'A' && code <= U'Z') || (code >= U'a' && code <= U'z');
    }
    return han ? QStringLiteral("zh-CN") : latin ? QStringLiteral("en") : QString();
}

QString HoverPolicy::lineAt(const QVector<OcrLine> &lines, QPoint point, float minimumConfidence)
{
    QString choice;
    int bestDistance = std::numeric_limits<int>::max();
    for (const auto &line : lines) {
        // A nearby paragraph is not the same as text under the pointer.
        if (line.confidence < minimumConfidence || line.text.trimmed().isEmpty()
            || !line.bounds.adjusted(-4, -5, 4, 5).contains(point))
            continue;
        const int distance = qAbs(line.bounds.center().y() - point.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            choice = line.text.simplified();
        }
    }
    return choice;
}
