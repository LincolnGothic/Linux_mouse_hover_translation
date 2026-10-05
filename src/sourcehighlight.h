// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
class SourceHighlight : public QWidget {
public:
    SourceHighlight();
    void showBoxes(const QVector<QRect> &boxes);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QVector<QRect> m_boxes;
};
