// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "translationpopup.h"
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

TranslationPopup::TranslationPopup()
    : QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setObjectName("translationPopup");
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setFixedWidth(390);
    setStyleSheet("QFrame#translationPopup { background: #f7faf9; border: 1px solid #b8d1c8; border-radius: 10px; }"
                  "QLabel { background: transparent; border: none; color: #15342b; }");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 12);
    layout->setSpacing(10);
    m_direction = new QLabel(this);
    m_direction->setStyleSheet("font-weight: 600; color: #28755c;");
    m_original = new QLabel(this);
    m_original->setStyleSheet("color: #60756d;");
    m_translation = new QLabel(this);
    m_translation->setStyleSheet("font-size: 17px; font-weight: 500;");
    m_hint = new QLabel(tr("Move the pointer or press Esc to dismiss"), this);
    m_hint->setStyleSheet("font-size: 11px; color: #70827b;");
    for (auto *label : {m_direction, m_original, m_translation, m_hint}) {
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        layout->addWidget(label);
    }
}

void TranslationPopup::showTranslation(QPoint pointer, const QString &source, const QString &translated,
                                      const QString &target, bool error)
{
    m_direction->setText(error ? tr("Translation unavailable")
        : target == "en" ? tr("Chinese → English") : tr("English → 简体中文"));
    m_original->setText(source.left(400));
    m_translation->setText(translated.left(1200));
    m_translation->setStyleSheet(error ? "color: #995424;" : "font-size: 17px; font-weight: 500;");
    adjustSize();
    const auto *screen = QGuiApplication::screenAt(pointer);
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    QPoint location = pointer + QPoint(16, 22);
    if (location.x() + width() > available.right()) location.setX(available.right() - width() + 1);
    if (location.y() + height() > available.bottom()) location.setY(pointer.y() - height() - 14);
    location.setX(qMax(available.left(), location.x()));
    location.setY(qMax(available.top(), location.y()));
    move(location);
    show();
}
