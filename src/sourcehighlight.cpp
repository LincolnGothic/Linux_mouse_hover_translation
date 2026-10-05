// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sourcehighlight.h"
#include <QPainter>
SourceHighlight::SourceHighlight() : QWidget(nullptr,Qt::ToolTip|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_ShowWithoutActivating); setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground); setFocusPolicy(Qt::NoFocus);
}
void SourceHighlight::showBoxes(const QVector<QRect> &boxes)
{
    hide(); if (boxes.isEmpty()) return;
    QRect area; for (const auto &box:boxes) area=area.united(box.adjusted(-2,-2,2,2));
    m_boxes.clear(); for (const auto &box:boxes) m_boxes.append(box.translated(-area.topLeft()));
    setGeometry(area); show(); update();
}
void SourceHighlight::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setPen(QPen(QColor("#218561"),1)); painter.setBrush(QColor(50,180,125,45));
    for (const auto &box:m_boxes) painter.drawRect(box);
}
