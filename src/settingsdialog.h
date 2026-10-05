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
    void setHoverAvailability(const QString &problem, bool enabled);
signals:
    void applyRequested(const HoverSettings &settings);
    void testRequested(const HoverSettings &settings);
    void captureRequested(const HoverSettings &settings);
    void readerRequested(const HoverSettings &settings);
    void setupRequested();
    void gnomeSetupRequested();
private:
    QCheckBox *m_enabled;
    QCheckBox *m_useDictionary;
    QComboBox *m_target;
    QComboBox *m_provider;
    QLineEdit *m_server, *m_tessdata;
    QLineEdit *m_python, *m_packages, *m_dictionary;
    QSpinBox *m_dwell;
    QLabel *m_status;
    QLabel *m_privacy;
};
