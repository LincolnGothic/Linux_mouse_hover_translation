// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#ifdef Q_OS_WIN
#include <QTimer>
#else
#include <QSocketNotifier>
#include <xcb/xcb.h>
#endif

class X11Escape : public QObject {
    Q_OBJECT
public:
    explicit X11Escape(QObject *parent = nullptr);
    ~X11Escape() override;
    bool grab();
    void release();
#ifndef Q_OS_WIN
    xcb_keycode_t keycode() const { return m_key; }
#endif
signals:
    void pressed();
private:
#ifdef Q_OS_WIN
    QTimer m_poll;
    bool m_wasDown = false;
#else
    xcb_connection_t *m_connection = nullptr;
    xcb_window_t m_root = XCB_WINDOW_NONE;
    xcb_keycode_t m_key = 0;
    QSocketNotifier *m_notifier = nullptr;
#endif
    bool m_grabbed = false;
};
