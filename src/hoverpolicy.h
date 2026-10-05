// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

struct OcrSymbol {
    QString text;
    QRect bounds;
    float confidence = 0;
    int offset = -1;
};
struct OcrWord {
    QString text;
    QRect bounds;
    float confidence = 0;
    int offset = -1;
    QVector<OcrSymbol> symbols;
};
struct OcrLine {
    QString text;
    QRect bounds;
    float confidence = 0;
    QVector<OcrWord> words;
    int block = -1;
    int paragraph = -1;
};
struct OcrResult {
    QVector<OcrLine> lines;
    QString error;
    bool canceled = false;
};

class HoverPolicy {
public:
    enum Action { Idle, Invalidated, Capture };
    Action update(QPoint position, bool blocked, qint64 nowMs);
    void reset();
    void dismiss() { ++m_generation; m_attempted = true; }
    void setDwellMs(int ms) { m_dwellMs = ms; reset(); }
    quint64 generation() const { return m_generation; }
    QPoint anchor() const { return m_anchor; }
    static bool moved(QPoint first, QPoint second);
    static QString sourceLanguage(const QString &text);
    static QVector<OcrLine> textRuns(const QVector<OcrLine> &lines);
    static QVector<QRect> sourceBounds(const QVector<OcrLine> &lines, QPoint point, const QString &text);
    static QString lineAt(const QVector<OcrLine> &lines, QPoint point, float minimumConfidence = 50);
    static QString textAt(const QVector<OcrLine> &lines, QPoint point, const QString &mode, float minimumConfidence = 50);
private:
    QPoint m_anchor;
    qint64 m_since = 0;
    quint64 m_generation = 0;
    int m_dwellMs = 600;
    bool m_initialized = false;
    bool m_blocked = false;
    bool m_attempted = false;
};
