#pragma once

#include "HotkeyTypes.h"

#include <QObject>
#include <QVector>

class RuleStore : public QObject {
    Q_OBJECT

public:
    explicit RuleStore(QObject* parent = nullptr);

    LauncherDocument loadDocument(QString* error = nullptr) const;
    bool saveDocument(const LauncherDocument& document, QString* error = nullptr) const;
    QVector<HotkeyRule> load(QString* error = nullptr) const;
    bool save(const QVector<HotkeyRule>& rules, QString* error = nullptr) const;
    QString configPath() const;
    QStringList warningsForRule(const HotkeyRule& rule, const QVector<HotkeyRule>& rules,
                                const QString& language = QString()) const;

private:
    LauncherDocument createDefaultDocument() const;
    void ensureDefaultSections(LauncherDocument* document) const;
    void ensureDefaultRules(LauncherDocument* document) const;
    QString defaultSectionId(LauncherCategory category, int index = 0) const;
    QString configDirectory() const;
};
