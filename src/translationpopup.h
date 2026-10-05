// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFrame>
class QLabel;
class QPushButton;
class TranslationPopup : public QFrame {
    Q_OBJECT
public:
    TranslationPopup();
    void showTranslation(QPoint pointer, const QString &source, const QString &translated,
                         const QString &target, bool error = false);
    bool pinned() const { return m_pinned; }
    void dismiss();
signals:
    void dismissed();
private:
    bool m_pinned = false;
    QPushButton *m_pin;
    QLabel *m_direction, *m_original, *m_translation, *m_hint;
};
