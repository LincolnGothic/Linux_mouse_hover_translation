// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "x11escape.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

X11Escape::X11Escape(QObject *parent) : QObject(parent)
{
    m_poll.setInterval(15);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        const bool down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool pressedNow = down && !m_wasDown;
        m_wasDown = down;
        if (pressedNow && m_grabbed) emit pressed();
    });
}

X11Escape::~X11Escape() { release(); }

bool X11Escape::grab()
{
    m_wasDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    m_grabbed = true; m_poll.start(); return true;
}

void X11Escape::release() { m_poll.stop(); m_grabbed = false; }
#else
#include <cstdlib>

X11Escape::X11Escape(QObject *parent) : QObject(parent)
{
    m_connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(m_connection)) {
        xcb_disconnect(m_connection);
        m_connection = nullptr;
        return;
    }
    const auto *setup = xcb_get_setup(m_connection);
    const auto screen = xcb_setup_roots_iterator(setup);
    if (!screen.rem) {
        xcb_disconnect(m_connection);
        m_connection = nullptr;
        return;
    }
    m_root = screen.data->root;
    const auto count = setup->max_keycode - setup->min_keycode + 1;
    auto *mapping = xcb_get_keyboard_mapping_reply(m_connection,
        xcb_get_keyboard_mapping(m_connection, setup->min_keycode, count), nullptr);
    if (mapping) {
        const auto *symbols = xcb_get_keyboard_mapping_keysyms(mapping);
        for (int code = setup->min_keycode; code <= setup->max_keycode && !m_key; ++code)
            for (int column = 0; column < mapping->keysyms_per_keycode; ++column)
                if (symbols[(code - setup->min_keycode) * mapping->keysyms_per_keycode + column] == 0xff1b) {
                    m_key = code;
                    break;
                }
        std::free(mapping);
    }
    m_notifier = new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        while (auto *event = xcb_poll_for_event(m_connection)) {
            const bool escape = (event->response_type & 0x7f) == XCB_KEY_PRESS
                && reinterpret_cast<xcb_key_press_event_t *>(event)->detail == m_key;
            std::free(event);
            if (escape && m_grabbed) emit pressed();
        }
    });
}

X11Escape::~X11Escape()
{
    release();
    if (m_connection) xcb_disconnect(m_connection);
}

bool X11Escape::grab()
{
    if (m_grabbed) return true;
    if (!m_connection || !m_key) return false;
    auto *error = xcb_request_check(m_connection, xcb_grab_key_checked(m_connection, false,
        m_root, XCB_MOD_MASK_ANY, m_key, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC));
    m_grabbed = error == nullptr;
    std::free(error);
    xcb_flush(m_connection);
    return m_grabbed;
}

void X11Escape::release()
{
    if (!m_connection || !m_grabbed) return;
    xcb_ungrab_key(m_connection, m_key, m_root, XCB_MOD_MASK_ANY);
    xcb_flush(m_connection);
    m_grabbed = false;
}

#endif
