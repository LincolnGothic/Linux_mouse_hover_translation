// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QSocketNotifier>
#include <xcb/xcb.h>

class X11Escape : public QObject {
    Q_OBJECT
public:
    explicit X11Escape(QObject *parent = nullptr);
    ~X11Escape() override;
    bool grab();
    void release();
    xcb_keycode_t keycode() const { return m_key; }
signals:
    void pressed();
private:
    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = XCB_WINDOW_NONE;
    xcb_keycode_t m_key = 0;
    bool m_grabbed = false;
    QSocketNotifier *m_notifier = nullptr;
};
