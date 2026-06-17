#pragma once

#include "HotkeyEdit.h"
#include "HotkeyTypes.h"
#include "UiText.h"

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class RuleDialog : public QDialog {
    Q_OBJECT

public:
    explicit RuleDialog(QWidget *parent = nullptr);
    explicit RuleDialog(const QString &language, QWidget *parent = nullptr);

    void setContext(LauncherCategory category, const QString &sectionId);
    void setRule(const HotkeyRule &rule);
    HotkeyRule rule() const;

private slots:
    void browseTarget();
    void checkHotkeyOccupancy();

private:
    QString uiText(UiText::Key key) const;
    void updateUiForCategory();
    LaunchActionType actionTypeForCategory() const;

    HotkeyRule m_rule;
    QString m_language = "zh-CN";
    LauncherCategory m_category = LauncherCategory::Program;
    QString m_sectionId;
    HotkeyEdit *m_hotkeyEdit = nullptr;
    QLineEdit *m_targetEdit = nullptr;
    QLineEdit *m_argumentsEdit = nullptr;
    QLineEdit *m_workingDirectoryEdit = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QWidget *m_argumentsRow = nullptr;
    QWidget *m_workingDirectoryRow = nullptr;
};
