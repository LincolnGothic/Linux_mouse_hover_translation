// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hoverpolicy.h"
#include <QSet>
class WordDictionary {
public:
    void load(const QString &path);
    QString chineseAt(const QVector<OcrLine> &lines, QPoint point) const;
private:
    QSet<QString> m_words;
};
