// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"
#include "hovercontroller.h"
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
    auto *description = new QLabel(tr("English ↔ Simplified Chinese, with offline translation.\nAutomatic hover: GNOME with the extension, or X11."), this);
    description->setStyleSheet("color: #657970;");
    layout->addWidget(description);
    m_enabled = new QCheckBox(tr("Enable hover translation"), this);
    m_enabled->setObjectName("enabledCheckBox");
    m_enabled->setEnabled(HoverController::platformProblem().isEmpty());
    m_enabled->setToolTip(HoverController::platformProblem());
    layout->addWidget(m_enabled);
    auto *form = new QFormLayout;
    form->setSpacing(14);
    m_target = new QComboBox(this);
    m_target->setObjectName("targetComboBox");
    m_target->addItem(tr("Simplified Chinese / 简体中文"), "zh-CN");
    m_target->addItem(tr("English"), "en");
    form->addRow(tr("Translate into"), m_target);
    m_provider = new QComboBox(this);
    m_provider->setObjectName("providerComboBox");
    m_provider->addItem(tr("Offline — Argos / dictionary"), "offline");
    m_provider->addItem(tr("Online — Mozhi"), "mozhi");
    form->addRow(tr("Translation"), m_provider);
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
    m_python = new QLineEdit(this);
    m_python->setPlaceholderText(tr("Use the installed offline runtime"));
    form->addRow(tr("Python executable (optional)"), m_python);
    m_packages = new QLineEdit(this);
    m_packages->setPlaceholderText(tr("Use downloaded translation models"));
    form->addRow(tr("Translation model folder (optional)"), m_packages);
    m_dictionary = new QLineEdit(this);
    m_dictionary->setPlaceholderText(tr("Use downloaded CC-CEDICT"));
    form->addRow(tr("CC-CEDICT file (optional)"), m_dictionary);
    m_useDictionary = new QCheckBox(tr("Show dictionary definitions for matching words"), this);
    form->addRow(m_useDictionary);
    layout->addLayout(form);
    m_privacy = new QLabel(this);
    m_privacy->setWordWrap(true);
    m_privacy->setStyleSheet("color: #657970; font-size: 12px;");
    layout->addWidget(m_privacy);
    auto updateProvider = [this] {
        const bool local = m_provider->currentData().toString() == "offline";
        m_server->setEnabled(!local);
        for (auto *edit : {m_python, m_packages, m_dictionary}) edit->setEnabled(local);
        m_useDictionary->setEnabled(local);
        m_privacy->setText(local
            ? tr("Offline mode keeps OCR and translation on this computer. Download the free models once; no API key is needed. CC-CEDICT supplies Chinese word definitions and limited English reverse lookup.")
            : tr("Online mode sends recognized text to your chosen Mozhi server. Screenshots stay on this computer. Public servers may be unavailable."));
    };
    connect(m_provider, &QComboBox::currentIndexChanged, this, updateProvider);
    updateProvider();
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setObjectName("statusLabel");
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    auto *test = buttons->addButton(tr("Test translation"), QDialogButtonBox::ActionRole);
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
        browser->setHtml(tr("<h2>Hover Translate 0.3.0</h2>"
            "<p>An open-source Linux hover translator based on Crow Translate 4.1.0.</p>"
            "<p>Copyright © 2026 Linux_mouse_hover_translation contributors.<br>"
            "Crow components: © 2018 Hennadii Chernyshchyk, © 2022 Volk Milit, and © 2026 Mauritius Clemens.</p>"
            "<p>Licensed under GNU GPL version 3 or later. There is no warranty."
            " You may redistribute and modify this program under that license.</p>"
            "<p><a href='https://github.com/LincolnGothic/Linux_mouse_hover_translation'>Source code and build instructions</a>"
            " · <a href='https://github.com/KDE/crow-translate/tree/v4.1.0'>Crow upstream</a></p>"
            "<p>Offline engine: Argos Translate (MIT), with models downloaded separately. "
            "Optional CC-CEDICT data retains its attribution/share-alike license.</p>"));
        QFile license(":/licenses/GPL-3.0-or-later.txt");
        if (license.open(QIODevice::ReadOnly)) browser->append(QString::fromUtf8(license.readAll()).toHtmlEscaped().replace("\n", "<br>"));
        aboutLayout->addWidget(browser);
        auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
        aboutLayout->addWidget(close);
        dialog.exec();
    });
    layout->addWidget(buttons);
    auto *actions = new QDialogButtonBox(this);
    auto *capture = actions->addButton(tr("Translate screen region"), QDialogButtonBox::ActionRole);
    capture->setObjectName("regionButton");
    auto *reader = actions->addButton(tr("Translate text"), QDialogButtonBox::ActionRole);
    auto *setup = actions->addButton(tr("Install offline models"), QDialogButtonBox::ActionRole);
    auto *gnomeSetup = actions->addButton(tr("Set up GNOME hover"), QDialogButtonBox::ActionRole);
    layout->addWidget(actions);
    connect(capture, &QPushButton::clicked, this, [this] { emit captureRequested(settings()); });
    connect(reader, &QPushButton::clicked, this, [this] { emit readerRequested(settings()); });
    connect(setup, &QPushButton::clicked, this, &SettingsDialog::setupRequested);
    connect(gnomeSetup, &QPushButton::clicked, this, &SettingsDialog::gnomeSetupRequested);
}

void SettingsDialog::setSettings(const HoverSettings &settings)
{
    m_enabled->setChecked(settings.enabled);
    m_target->setCurrentIndex(settings.target == "en" ? 1 : 0);
    m_provider->setCurrentIndex(settings.provider == "mozhi" ? 1 : 0);
    m_server->setText(settings.instance);
    m_dwell->setValue(settings.dwellMs);
    m_tessdata->setText(settings.tessdataPath);
    m_python->setText(settings.pythonPath);
    m_packages->setText(settings.packagesPath);
    m_dictionary->setText(settings.dictionaryPath);
    m_useDictionary->setChecked(settings.useDictionary);
}

HoverSettings SettingsDialog::settings() const
{
    HoverSettings settings;
    settings.enabled = m_enabled->isChecked();
    settings.target = m_target->currentData().toString();
    settings.provider = m_provider->currentData().toString();
    settings.instance = m_server->text().trimmed();
    settings.dwellMs = m_dwell->value();
    settings.tessdataPath = m_tessdata->text().trimmed();
    settings.pythonPath = m_python->text().trimmed();
    settings.packagesPath = m_packages->text().trimmed();
    settings.dictionaryPath = m_dictionary->text().trimmed();
    settings.useDictionary = m_useDictionary->isChecked();
    return settings;
}
void SettingsDialog::setStatus(const QString &status) { m_status->setText(status); }
void SettingsDialog::showProblem(const QString &problem) { QMessageBox::warning(this, tr("Hover Translate"), problem); }
void SettingsDialog::setHoverAvailability(const QString &problem, bool enabled)
{
    m_enabled->setEnabled(problem.isEmpty());
    m_enabled->setToolTip(problem);
    m_enabled->setChecked(enabled);
}
