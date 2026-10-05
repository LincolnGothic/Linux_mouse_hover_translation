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
#include <QScrollArea>
#include <QScreen>
#include <QTabWidget>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Hover Translate — Settings"));
    setMinimumSize(400, 380);
    const auto available = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(640, available.width() - 40), qMin(700, available.height() - 80));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 16, 18, 16);
    outer->setSpacing(12);
    auto *title = new QLabel(tr("Settings"), this);
    title->setStyleSheet("font-size: 23px; font-weight: 600;");
    outer->addWidget(title);
    auto *description = new QLabel(tr("English and Simplified Chinese. Choose how you translate."), this);
    description->setWordWrap(true);
    outer->addWidget(description);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("settingsTabs");
    outer->addWidget(tabs, 1);
    auto page = [tabs](const QString &name, const QString &objectName) {
        auto *scroll = new QScrollArea(tabs);
        scroll->setObjectName(objectName);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *content = new QWidget(scroll);
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        scroll->setWidget(content);
        tabs->addTab(scroll, name);
        return layout;
    };
    auto *hoverLayout = page(tr("Hover"), "hoverSettingsScroll");
    auto *translationLayout = page(tr("Translation"), "translationSettingsScroll");
    auto *advancedLayout = page(tr("Advanced"), "advancedSettingsScroll");
    auto paragraph = [](const QString &text, QVBoxLayout *layout) {
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        layout->addWidget(label);
        return label;
    };
    auto form = [](QVBoxLayout *layout) {
        auto *rows = new QFormLayout;
        rows->setRowWrapPolicy(QFormLayout::WrapAllRows);
        rows->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        rows->setVerticalSpacing(8);
        layout->addLayout(rows);
        return rows;
    };
    auto combo = [this](const QString &name) {
        auto *field = new QComboBox(this);
        field->setObjectName(name);
        field->setMinimumContentsLength(16);
        field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return field;
    };
    m_enabled = new QCheckBox(tr("Enable hover translation"), this);
    m_enabled->setObjectName("enabledCheckBox");
    m_enabled->setEnabled(HoverController::platformProblem().isEmpty());
    m_enabled->setToolTip(HoverController::platformProblem());
    hoverLayout->addWidget(m_enabled);
    auto *hoverForm = form(hoverLayout);
    m_target = combo("targetComboBox");
    m_target->addItem(tr("Simplified Chinese / 简体中文"), "zh-CN");
    m_target->addItem(tr("English"), "en");
    hoverForm->addRow(tr("Translate into"), m_target);
    m_textMode = combo("textModeComboBox");
    m_textMode->addItem(tr("Word — under the pointer"), "word");
    m_textMode->addItem(tr("Line — nearby text on this line"), "line");
    m_textMode->addItem(tr("Sentence — up to 3 nearby lines"), "sentence");
    m_textMode->setToolTip(tr("Sentence mode stays within one paragraph and column, up to 3 lines and 300 characters. If boundaries are uncertain, it uses the current line. Large horizontal gaps separate text. Chinese lookup uses CC-CEDICT when available."));
    hoverForm->addRow(tr("Hover text"), m_textMode);
    m_dwell = new QSpinBox(this);
    m_dwell->setRange(100, 3000);
    m_dwell->setSingleStep(100);
    m_dwell->setSuffix(" ms");
    hoverForm->addRow(tr("Hover delay"), m_dwell);
    m_highlight = new QCheckBox(tr("Highlight selected source text"), this);
    m_temporary = new QCheckBox(tr("Temporary mode shortcuts"), this);
    m_temporary->setToolTip(tr("Hold Shift for Word; Ctrl+Shift for Sentence. Release to restore your saved mode."));
    hoverLayout->addWidget(m_highlight);
    hoverLayout->addWidget(m_temporary);
    paragraph(tr("Hold Shift for Word, or Ctrl+Shift for Sentence. Release to return to your saved mode."), hoverLayout);
    auto *gnomeSetup = new QPushButton(tr("Set up GNOME hover"), this);
    gnomeSetup->setObjectName("gnomeSetupButton");
    hoverLayout->addWidget(gnomeSetup, 0, Qt::AlignLeft);
    paragraph(tr("Automatic hover works on GNOME 50 with the extension, or X11. After installing a new extension, sign out and back in."), hoverLayout);
    hoverLayout->addStretch();

    auto *translationForm = form(translationLayout);
    m_provider = combo("providerComboBox");
    m_provider->addItem(tr("Offline — Argos / dictionary"), "offline");
    m_provider->addItem(tr("Online — Mozhi"), "mozhi");
    translationForm->addRow(tr("Translation engine"), m_provider);
    m_server = new QLineEdit(this);
    m_server->setObjectName("serverLineEdit");
    m_server->setPlaceholderText("https://mozhi.aryak.me");
    translationForm->addRow(tr("Mozhi server"), m_server);
    m_useDictionary = new QCheckBox(tr("Show dictionary definitions"), this);
    translationLayout->addWidget(m_useDictionary);
    m_privacy = paragraph(QString(), translationLayout);
    auto *setup = new QPushButton(tr("Install offline models"), this);
    setup->setObjectName("offlineSetupButton");
    translationLayout->addWidget(setup, 0, Qt::AlignLeft);
    auto *test = new QPushButton(tr("Test translation"), this);
    test->setObjectName("testTranslationButton");
    translationLayout->addWidget(test, 0, Qt::AlignLeft);
    translationLayout->addStretch();
    auto updateProvider = [this] {
        const bool local = m_provider->currentData().toString() == "offline";
        m_server->setEnabled(!local);
        for (auto *edit : {m_python, m_packages, m_dictionary}) edit->setEnabled(local);
        m_useDictionary->setEnabled(local);
        m_privacy->setText(local
            ? tr("Offline mode keeps OCR and translation on this computer. Download the free models once; no API key is needed. CC-CEDICT supplies Chinese word definitions and limited English reverse lookup.")
            : tr("Online mode sends recognized text to your chosen Mozhi server. Screenshots stay on this computer. Public servers may be unavailable."));
    };

    paragraph(tr("Optional paths"), advancedLayout)->setStyleSheet("font-weight: 600;");
    paragraph(tr("Leave these empty to use the installed OCR languages and downloaded offline resources."), advancedLayout);
    auto *advancedForm = form(advancedLayout);
    m_tessdata = new QLineEdit(this);
    m_tessdata->setPlaceholderText(tr("Use installed language models"));
    advancedForm->addRow(tr("OCR model folder"), m_tessdata);
    m_python = new QLineEdit(this);
    m_python->setPlaceholderText(tr("Use the installed offline runtime"));
    advancedForm->addRow(tr("Python executable"), m_python);
    m_packages = new QLineEdit(this);
    m_packages->setPlaceholderText(tr("Use downloaded translation models"));
    advancedForm->addRow(tr("Translation model folder"), m_packages);
    m_dictionary = new QLineEdit(this);
    m_dictionary->setObjectName("dictionaryLineEdit");
    m_dictionary->setPlaceholderText(tr("Use downloaded CC-CEDICT"));
    advancedForm->addRow(tr("CC-CEDICT file"), m_dictionary);
    advancedLayout->addStretch();
    connect(m_provider, &QComboBox::currentIndexChanged, this, updateProvider);
    updateProvider();

    // Status and primary actions remain reachable when a page needs scrolling.
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setObjectName("statusLabel");
    outer->addWidget(m_status);
    auto *tools = new QHBoxLayout;
    auto *capture = new QPushButton(tr("Screen region"), this);
    capture->setObjectName("regionButton");
    capture->setToolTip(tr("Capture a screen region and translate its text"));
    auto *reader = new QPushButton(tr("Translate text"), this);
    reader->setObjectName("textButton");
    tools->addWidget(capture);
    tools->addWidget(reader);
    outer->addLayout(tools);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    buttons->setObjectName("settingsButtonBox");
    buttons->button(QDialogButtonBox::Apply)->setObjectName("applyButton");
    buttons->button(QDialogButtonBox::Close)->setObjectName("closeButton");
    auto *about = buttons->addButton(tr("About"), QDialogButtonBox::ActionRole);
    about->setToolTip(tr("About Hover Translate and its licenses"));
    outer->addWidget(buttons);
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
        browser->setHtml(tr("<h2>Hover Translate 0.4.2</h2>"
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
    connect(capture, &QPushButton::clicked, this, [this] { emit captureRequested(settings()); });
    connect(reader, &QPushButton::clicked, this, [this] { emit readerRequested(settings()); });
    connect(setup, &QPushButton::clicked, this, &SettingsDialog::setupRequested);
    connect(gnomeSetup, &QPushButton::clicked, this, &SettingsDialog::gnomeSetupRequested);
}

void SettingsDialog::setSettings(const HoverSettings &settings)
{
    m_enabled->setChecked(settings.enabled);
    m_highlight->setChecked(settings.highlightSource);
    m_temporary->setChecked(settings.temporaryModes);
    m_target->setCurrentIndex(settings.target == "en" ? 1 : 0);
    m_provider->setCurrentIndex(settings.provider == "mozhi" ? 1 : 0);
    m_textMode->setCurrentIndex(m_textMode->findData(settings.textMode));
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
    settings.highlightSource = m_highlight->isChecked();
    settings.temporaryModes = m_temporary->isChecked();
    settings.target = m_target->currentData().toString();
    settings.provider = m_provider->currentData().toString();
    settings.textMode = m_textMode->currentData().toString();
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
