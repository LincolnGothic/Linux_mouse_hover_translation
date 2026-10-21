// SPDX-FileCopyrightText: 2026 Hover Translate contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include <QPalette>
#include <QWidget>

void applyAppStyle(QWidget *widget)
{
    const bool dark = widget->palette().color(QPalette::Window).lightness() < 128;
    QString css = QStringLiteral(R"(
        QDialog, QFrame#translationPopup { background: @window; color: @text; }
        QLabel, QCheckBox { color: @text; }
        QLabel[role="muted"] { color: @muted; }
        QLabel[role="section"] { color: @accent; font-weight: 600; }
        QLabel#appTitle { font-size: 25px; font-weight: 700; }
        QLabel#platformBadge { background: @tint; color: @accent; border-radius: 8px; padding: 5px 9px; }
        QLabel#statusLabel { background: @tint; color: @text; border-radius: 10px; padding: 10px 12px; }
        QTabWidget::pane { border: 1px solid @border; border-radius: 10px; background: @surface; }
        QTabBar::tab { background: @window; color: @muted; padding: 10px 16px; border-bottom: 3px solid transparent; }
        QTabBar::tab:selected { color: @accent; border-bottom: 3px solid @accent; font-weight: 600; }
        QTabBar::tab:hover { background: @tint; }
        QScrollArea, QWidget#settingsPage { background: @surface; border: none; }
        QLineEdit, QComboBox, QSpinBox, QPlainTextEdit {
            background: @surface; color: @text; border: 1px solid @border;
            border-radius: 7px; padding: 7px 10px; selection-background-color: @accent;
        }
        QComboBox, QSpinBox { padding-right: 22px; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus { border: 1px solid @accent; }
        QCheckBox { spacing: 9px; padding: 3px 0; }
        QPushButton { background: @surface; color: @text; border: 1px solid @border; border-radius: 7px; padding: 8px 13px; }
        QPushButton:hover { background: @tint; border-color: @accent; }
        QPushButton:pressed { background: @border; }
        QPushButton:focus { border: 2px solid @accent; padding: 7px 12px; }
        QPushButton[primary="true"] { background: @accent; color: @onAccent; border-color: @accent; font-weight: 600; }
        QPushButton[primary="true"]:hover { background: @accentHover; }
        QPushButton[primary="true"]:focus { border: 2px solid @text; }
        QPushButton:disabled, QCheckBox:disabled, QLineEdit:disabled { color: @muted; }
        QFrame#translationPopup { border: 1px solid @border; border-radius: 14px; }
        QLabel#popupDirection { color: @accent; font-weight: 600; }
        QLabel#popupResult { font-size: 18px; font-weight: 500; }
        QLabel#popupResult[error="true"] { color: @error; }
    )");
    css.replace("@window", dark ? "#162024" : "#f4f8f7");
    css.replace("@surface", dark ? "#1d2a2e" : "#ffffff");
    css.replace("@text", dark ? "#e3eeeb" : "#183c34");
    css.replace("@muted", dark ? "#acc2bc" : "#536f67");
    css.replace("@border", dark ? "#3c504d" : "#d6e3df");
    css.replace("@tint", dark ? "#233b35" : "#e7f2ed");
    css.replace("@onAccent", dark ? "#09291e" : "#ffffff");
    css.replace("@accentHover", dark ? "#78e0ba" : "#0d5c47");
    css.replace("@accent", dark ? "#57d2a7" : "#14775b");
    css.replace("@error", dark ? "#ffbb9a" : "#a34224");
    widget->setStyleSheet(css);
}
