// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hoverpolicy.h"
#include <QChar>
#include <QRegularExpression>
#include <limits>
#include <algorithm>

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

QVector<OcrLine> HoverPolicy::textRuns(const QVector<OcrLine> &lines)
{
    QVector<OcrLine> runs;
    for (const auto &line : lines) {
        if (line.words.size() < 2) { runs.append(line); continue; }
        auto words = line.words;
        std::sort(words.begin(), words.end(), [](const auto &a, const auto &b) { return a.bounds.left() < b.bounds.left(); });
        QVector<double> widths;
        for (const auto &word : words) if (!word.text.isEmpty()) widths.append(double(word.bounds.width()) / word.text.size());
        std::sort(widths.begin(), widths.end());
        const double letterWidth = widths.isEmpty() ? 0 : widths[widths.size()/2];
        const int maximumGap = qMax(24, qMax(line.bounds.height() * 2, int(letterWidth * 4)));
        bool separated = false;
        for (int i = 1; i < words.size(); ++i)
            separated |= words[i].bounds.left() - words[i-1].bounds.right() - 1 > maximumGap;
        if (!separated) { runs.append(line); continue; }
        int begin = 0;
        auto append = [&](int end) {
            OcrLine run; run.confidence = line.confidence; run.block = line.block; run.paragraph = line.paragraph;
            const int offset = words[begin].offset;
            const int finish = words[end-1].offset + words[end-1].text.size();
            bool validOffsets = offset >= 0 && finish <= line.text.size();
            int previous = offset;
            for (int i = begin; i < end; ++i) {
                validOffsets &= words[i].offset >= previous;
                previous = words[i].offset + words[i].text.size();
            }
            if (validOffsets) run.text = line.text.mid(offset, finish-offset);
            for (int i = begin; i < end; ++i) {
                auto word = words[i];
                if (!validOffsets) {
                    if (!run.text.isEmpty()) run.text += ' ';
                    word.offset = run.text.size(); run.text += word.text;
                } else {
                    word.offset -= offset;
                    for (auto &symbol : word.symbols) if (symbol.offset >= 0) symbol.offset -= offset;
                }
                run.words.append(word); run.bounds = run.bounds.united(word.bounds);
            }
            runs.append(run);
        };
        for (int i = 1; i < words.size(); ++i) if (words[i].bounds.left() - words[i-1].bounds.right() - 1 > maximumGap) {
            append(i); begin = i;
        }
        append(words.size());
    }
    // Runs from adjacent table cells may be interleaved in OCR reading order.
    std::stable_sort(runs.begin(), runs.end(), [](const auto &a, const auto &b) {
        return a.bounds.center().y() < b.bounds.center().y();
    });
    return runs;
}

QString HoverPolicy::lineAt(const QVector<OcrLine> &input, QPoint point, float minimumConfidence)
{
    const auto lines = textRuns(input);
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

QString HoverPolicy::textAt(const QVector<OcrLine> &input, QPoint point, const QString &mode, float minimumConfidence)
{
    if (mode == "line") return lineAt(input, point, minimumConfidence);
    const auto lines = textRuns(input);
    int anchor = -1, distance = std::numeric_limits<int>::max();
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        if (line.confidence < minimumConfidence || line.text.trimmed().isEmpty()
            || !line.bounds.adjusted(-4, -5, 4, 5).contains(point)) continue;
        const int d = qAbs(line.bounds.center().y() - point.y());
        if (d < distance) { anchor = i; distance = d; }
    }
    if (anchor < 0) return {};
    const auto &line = lines[anchor];
    const OcrWord *hit = nullptr;
    for (const auto &word : line.words) {
        if (word.confidence < minimumConfidence || !word.bounds.adjusted(-1, -2, 1, 2).contains(point)) continue;
        if (!hit || word.bounds.contains(point)) hit = &word;
        if (word.bounds.contains(point)) break;
    }
    if (mode == "word") {
        if (!hit) return {};
        QString word = hit->text;
        while (!word.isEmpty() && !word.front().isLetterOrNumber()) word.remove(0, 1);
        while (!word.isEmpty() && !word.back().isLetterOrNumber()) word.chop(1);
        return word;
    }
    if (mode != "sentence") return {};
    const auto fallback = [&]() { return line.text.size() <= 300 ? line.text.simplified() : QString(); };
    // Without word geometry, a line with several sentences has no reliable
    // pointer-to-sentence mapping. Never guess a character from its x ratio.
    if (!hit || hit->offset < 0) return fallback();
    static const QRegularExpression boundary(QStringLiteral(
        R"([。！？]+["'”’）)]*\s*|[.!?]+["'”’）)]*(?:\s+|$))"));
    auto endsSentence = [&](const QString &text) {
        auto matches = boundary.globalMatch(text.trimmed());
        while (matches.hasNext()) {
            const auto match = matches.next();
            if (match.capturedEnd() == text.trimmed().size()) return true;
        }
        return false;
    };
    auto adjacent = [&](int upper, int lower) {
        const auto &a = lines[upper], &b = lines[lower];
        if (a.block < 0 || a.paragraph < 0 || a.block != b.block || a.paragraph != b.paragraph
            || a.confidence < minimumConfidence || b.confidence < minimumConfidence) return false;
        const int height = qMax(a.bounds.height(), b.bounds.height());
        const int gap = b.bounds.top() - a.bounds.bottom() - 1;
        const int overlap = qMin(a.bounds.right(), b.bounds.right()) - qMax(a.bounds.left(), b.bounds.left()) + 1;
        return b.bounds.center().y() > a.bounds.center().y() && gap >= -2 && gap <= height
            && qMin(a.bounds.height(), b.bounds.height()) * 2 >= height
            && qAbs(a.bounds.left() - b.bounds.left()) <= height * 2
            && overlap * 2 >= qMin(a.bounds.width(), b.bounds.width());
    };
    int first = anchor, last = anchor;
    while (first > 0 && adjacent(first - 1, first) && !endsSentence(lines[first - 1].text)) {
        if (last - first + 1 == 3) return fallback();
        --first;
    }
    QString joined;
    int pointerOffset = 0;
    auto append = [&](int index) {
        const auto text = lines[index].text.simplified();
        if (!joined.isEmpty()) {
            const bool dehyphenate = joined.endsWith('-') && joined.size() > 1
                && joined[joined.size() - 2].isLetter() && !text.isEmpty() && text.front().isLower();
            if (dehyphenate) joined.chop(1);
            else if (!(joined.back().script() == QChar::Script_Han && !text.isEmpty()
                       && text.front().script() == QChar::Script_Han)) joined += ' ';
        }
        if (index == anchor) pointerOffset = joined.size() + hit->offset;
        joined += text;
    };
    for (int i = first; i <= last; ++i) append(i);
    for (;;) {
        int start = 0, end = -1;
        auto matches = boundary.globalMatch(joined);
        while (matches.hasNext()) {
            const auto match = matches.next();
            if (match.capturedEnd() <= pointerOffset) start = match.capturedEnd();
            else { end = match.capturedEnd(); break; }
        }
        if (end >= 0) {
            const auto selected = joined.mid(start, end - start).trimmed();
            return selected.size() <= 300 ? selected : fallback();
        }
        if (last - first + 1 == 3 || last + 1 >= lines.size() || !adjacent(last, last + 1)) return fallback();
        append(++last);
    }
}

QVector<QRect> HoverPolicy::sourceBounds(const QVector<OcrLine> &lines, QPoint point, const QString &text)
{
    const auto runs = textRuns(lines);
    const OcrLine *anchor = nullptr;
    for (const auto &run : runs) if (run.bounds.adjusted(-1,-2,1,2).contains(point)) { anchor = &run; break; }
    if (!anchor || text.isEmpty()) return {};
    QString haystack, needle; QVector<QRect> characters;
    for (const auto ch:text) if (!ch.isSpace() && ch != '-') needle += ch;
    for (const auto &run:runs) {
        if (&run != anchor && (run.block != anchor->block || run.paragraph != anchor->paragraph
            || run.block < 0 || qAbs(run.bounds.left()-anchor->bounds.left())>anchor->bounds.height()*2
            || qAbs(run.bounds.center().y()-anchor->bounds.center().y())>anchor->bounds.height()*6)) continue;
        for (int i=0;i<run.text.size();++i) {
            const auto ch=run.text[i]; if (ch.isSpace() || ch=='-') continue;
            QRect box;
            for (const auto &word:run.words) if (word.offset>=0 && i>=word.offset && i<word.offset+word.text.size()) {
                box=word.bounds;
                for (const auto &symbol:word.symbols) if (symbol.offset==i) { box=symbol.bounds; break; }
                break;
            }
            haystack += ch; characters.append(box);
        }
    }
    int position=-1;
    for (int from=0;from<haystack.size();) {
        const int found=haystack.indexOf(needle,from); if (found<0) break;
        bool containsPointer=false; QRect row;
        for (int i=found;i<found+needle.size();++i) {
            const auto box=characters[i]; if (box.isEmpty()) continue;
            if (!row.isEmpty() && qAbs(row.center().y()-box.center().y())>qMax(2,box.height()/2)) {
                containsPointer |= row.adjusted(-1,-2,1,2).contains(point); row={};
            }
            row=row.united(box);
        }
        containsPointer |= row.adjusted(-1,-2,1,2).contains(point);
        if (containsPointer) { position=found; break; }
        from=found+1;
    }
    QVector<QRect> boxes;
    if (position<0) return text==anchor->text.simplified() ? QVector<QRect>{anchor->bounds} : boxes;
    for (int i=position;i<position+needle.size();++i) {
        const auto box=characters[i]; if (box.isEmpty()) continue;
        if (!boxes.isEmpty() && qAbs(boxes.last().center().y()-box.center().y())<=qMax(2,box.height()/3)
            && box.left()<=boxes.last().right()+4) boxes.last()=boxes.last().united(box);
        else boxes.append(box);
    }
    return boxes;
}
