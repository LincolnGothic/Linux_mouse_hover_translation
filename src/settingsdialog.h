// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings.h"
#include <QDialog>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    void setSettings(const HoverSettings &settings);
    HoverSettings settings() const;
    void setStatus(const QString &status);
    void showProblem(const QString &problem);
signals:
    void applyRequested(const HoverSettings &settings);
    void testRequested(const HoverSettings &settings);
private:
    QCheckBox *m_enabled;
    QComboBox *m_target;
    QLineEdit *m_server, *m_tessdata;
    QSpinBox *m_dwell;
    QLabel *m_status;
};
