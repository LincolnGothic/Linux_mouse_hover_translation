// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "translationpopup.h"
#include "appearance.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

TranslationPopup::TranslationPopup()
    : QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setObjectName("translationPopup");
    setAttribute(Qt::WA_ShowWithoutActivating);

    setFocusPolicy(Qt::NoFocus);
    setFixedWidth(390);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 12);
    layout->setSpacing(10);
    m_direction = new QLabel(this);
    m_direction->setObjectName("popupDirection");
    m_original = new QLabel(this);
    m_original->setProperty("role", "muted");
    m_translation = new QLabel(this);
    m_translation->setObjectName("popupResult");
    m_hint = new QLabel(tr("Move into this popup to copy or pin. Esc closes it."), this);
    m_hint->setProperty("role", "muted");
    for (auto *label : {m_direction, m_original, m_translation, m_hint}) {
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        layout->addWidget(label);
    }
    auto *actions = new QHBoxLayout;
    auto *copy = new QPushButton(tr("Copy translation"),this); copy->setObjectName("copyTranslationButton");
    m_pin = new QPushButton(tr("Pin"),this); m_pin->setObjectName("pinTranslationButton"); m_pin->setCheckable(true);
    auto *close = new QPushButton(tr("Close"),this); close->setObjectName("closeTranslationButton");
    for (auto *button:{copy,m_pin,close}) { button->setFocusPolicy(Qt::NoFocus); actions->addWidget(button); }
    layout->addLayout(actions);
    connect(copy,&QPushButton::clicked,this,[this] { QGuiApplication::clipboard()->setText(m_translation->text()); });
    connect(m_pin,&QPushButton::toggled,this,[this](bool pinned) { m_pinned=pinned; m_pin->setText(pinned?tr("Unpin"):tr("Pin")); });
    connect(close,&QPushButton::clicked,this,[this] { dismiss(); emit dismissed(); });
    applyAppStyle(this);
}

void TranslationPopup::showTranslation(QPoint pointer, const QString &source, const QString &translated,
                                      const QString &target, bool error)
{
    m_pin->setChecked(false);
    m_direction->setText(error ? tr("Translation unavailable")
        : target == "en" ? tr("Chinese → English") : tr("English → 简体中文"));
    m_original->setText(source.left(400));
    m_translation->setText(translated.left(1200));
    m_translation->setProperty("error", error);
    m_translation->style()->unpolish(m_translation);
    m_translation->style()->polish(m_translation);
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

void TranslationPopup::dismiss() { m_pin->setChecked(false); hide(); }
