// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QUrl>

struct HoverSettings {
    QString target = QStringLiteral("zh-CN");
    QString instance = QStringLiteral("https://mozhi.aryak.me");
    int dwellMs = 600;
    bool enabled = false;
    QString tessdataPath;
};

class SettingsStore {
public:
    explicit SettingsStore(QString fileName = {});
    HoverSettings load() const;
    bool save(const HoverSettings &settings, QString *error = nullptr) const;
    const QString &fileName() const { return m_fileName; }
    static bool validate(const HoverSettings &settings, QString *error = nullptr);
    static bool validInstance(const QString &instance);
private:
    QString m_fileName;
};
