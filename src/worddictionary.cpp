// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "worddictionary.h"
#include <QFile>
#include <QTextStream>

void WordDictionary::load(const QString &path)
{
    m_words.clear(); if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const auto line = stream.readLine();
        if (line.startsWith('#')) continue;
        const auto fields = line.split(' ');
        if (fields.size() < 3 || !fields[2].startsWith('[')) continue;
        for (int i = 0; i < 2; ++i) if (!fields[i].isEmpty() && fields[i].size() <= 16) m_words.insert(fields[i]);
    }
}

QString WordDictionary::chineseAt(const QVector<OcrLine> &lines, QPoint point) const
{
    for (const auto &run : HoverPolicy::textRuns(lines)) {
        if (run.confidence < 50 || !run.bounds.adjusted(-1,-2,1,2).contains(point)) continue;
        const OcrSymbol *hit = nullptr;
        for (const auto &word : run.words) for (const auto &symbol : word.symbols)
            if (symbol.confidence >= 50 && symbol.bounds.adjusted(-1,-2,1,2).contains(point)
                && !symbol.text.isEmpty() && symbol.text.front().script() == QChar::Script_Han) {
                hit = &symbol; break;
            }
        if (!hit || hit->offset < 0) continue;
        // Remove only OCR-inserted whitespace. Punctuation/Latin text and
        // large geometric gaps remain hard word boundaries.
        QString han; QVector<int> offsets;
        for (int i=0;i<run.text.size();++i) if (!run.text[i].isSpace()) { han += run.text[i]; offsets.append(i); }
        const int anchor = offsets.indexOf(hit->offset);
        if (anchor < 0) continue;
        QString best;
        for (int start=qMax(0,anchor-15);start<=anchor;++start) for (int length=1;length<=16 && start+length<=han.size();++length) {
            if (start+length<=anchor || length<=best.size()) continue;
            const auto candidate=han.mid(start,length);
            bool allHan=true; for (const auto ch:candidate) allHan &= ch.script()==QChar::Script_Han;
            if (allHan && m_words.contains(candidate)) best=candidate;
        }
        return best;
    }
    return {};
}
