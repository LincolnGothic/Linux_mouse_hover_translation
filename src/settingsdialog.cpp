// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextBrowser>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Hover Translate"));
    setMinimumWidth(540);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 20);
    layout->setSpacing(18);
    auto *title = new QLabel(tr("Translate where you point"), this);
    title->setStyleSheet("font-size: 23px; font-weight: 600; color: #22634d;");
    layout->addWidget(title);
    auto *description = new QLabel(tr("Pause over a line to see its translation.\nEnglish and Simplified Chinese · X11 desktop"), this);
    description->setStyleSheet("color: #657970;");
    layout->addWidget(description);
    m_enabled = new QCheckBox(tr("Enable hover translation"), this);
    m_enabled->setObjectName("enabledCheckBox");
    layout->addWidget(m_enabled);
    auto *form = new QFormLayout;
    form->setSpacing(14);
    m_target = new QComboBox(this);
    m_target->setObjectName("targetComboBox");
    m_target->addItem(tr("Simplified Chinese / 简体中文"), "zh-CN");
    m_target->addItem(tr("English"), "en");
    form->addRow(tr("Translate into"), m_target);
    m_server = new QLineEdit(this);
    m_server->setObjectName("serverLineEdit");
    m_server->setPlaceholderText("https://mozhi.aryak.me");
    form->addRow(tr("Mozhi server"), m_server);
    m_dwell = new QSpinBox(this);
    m_dwell->setRange(100, 3000);
    m_dwell->setSingleStep(100);
    m_dwell->setSuffix(" ms");
    form->addRow(tr("Hover delay"), m_dwell);
    m_tessdata = new QLineEdit(this);
    m_tessdata->setPlaceholderText(tr("Use installed language models"));
    form->addRow(tr("OCR model folder (optional)"), m_tessdata);
    layout->addLayout(form);
    auto *privacy = new QLabel(tr("OCR runs locally. When hover is enabled, recognized text is sent to your chosen translation server. Screenshots stay on your computer. Public servers can be unavailable."), this);
    privacy->setWordWrap(true);
    privacy->setStyleSheet("color: #657970; font-size: 12px;");
    layout->addWidget(privacy);
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setObjectName("statusLabel");
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    auto *test = buttons->addButton(tr("Test server"), QDialogButtonBox::ActionRole);
    auto *about = buttons->addButton(tr("About & licenses"), QDialogButtonBox::ActionRole);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {
        QString error;
        if (!SettingsStore::validate(settings(), &error)) showProblem(error);
        else emit applyRequested(settings());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(test, &QPushButton::clicked, this, [this] {
        QString error;
        if (!SettingsStore::validate(settings(), &error)) showProblem(error);
        else emit testRequested(settings());
    });
    connect(about, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("About Hover Translate"));
        dialog.resize(650, 500);
        auto *aboutLayout = new QVBoxLayout(&dialog);
        auto *browser = new QTextBrowser(&dialog);
        browser->setOpenExternalLinks(true);
        browser->setHtml(tr("<h2>Hover Translate 0.1.0</h2>"
            "<p>An open-source Linux hover translator based on Crow Translate 4.1.0.</p>"
            "<p>Copyright © 2026 Linux_mouse_hover_translation contributors.<br>"
            "Crow components: © 2018 Hennadii Chernyshchyk, © 2022 Volk Milit, and © 2026 Mauritius Clemens.</p>"
            "<p>Licensed under GNU GPL version 3 or later. There is no warranty."
            " You may redistribute and modify this program under that license.</p>"
            "<p><a href='https://github.com/LincolnGothic/Linux_mouse_hover_translation'>Source code and build instructions</a>"
            " · <a href='https://github.com/KDE/crow-translate/tree/v4.1.0'>Crow upstream</a></p>"));
        QFile license(":/licenses/GPL-3.0-or-later.txt");
        if (license.open(QIODevice::ReadOnly)) browser->append(QString::fromUtf8(license.readAll()).toHtmlEscaped().replace("\n", "<br>"));
        aboutLayout->addWidget(browser);
        auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
        aboutLayout->addWidget(close);
        dialog.exec();
    });
    layout->addWidget(buttons);
}

void SettingsDialog::setSettings(const HoverSettings &settings)
{
    m_enabled->setChecked(settings.enabled);
    m_target->setCurrentIndex(settings.target == "en" ? 1 : 0);
    m_server->setText(settings.instance);
    m_dwell->setValue(settings.dwellMs);
    m_tessdata->setText(settings.tessdataPath);
}

HoverSettings SettingsDialog::settings() const
{
    HoverSettings settings;
    settings.enabled = m_enabled->isChecked();
    settings.target = m_target->currentData().toString();
    settings.instance = m_server->text().trimmed();
    settings.dwellMs = m_dwell->value();
    settings.tessdataPath = m_tessdata->text().trimmed();
    return settings;
}
void SettingsDialog::setStatus(const QString &status) { m_status->setText(status); }
void SettingsDialog::showProblem(const QString &problem) { QMessageBox::warning(this, tr("Hover Translate"), problem); }
