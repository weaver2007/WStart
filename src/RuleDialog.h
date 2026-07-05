#pragma once

#include "HotkeyTypes.h"
#include "UiText.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class RuleDialog : public QDialog {
    Q_OBJECT

public:
    explicit RuleDialog(QWidget* parent = nullptr);
    explicit RuleDialog(const QString& language, QWidget* parent = nullptr);

    void setContext(LauncherCategory category, const QString& sectionId);
    void setRule(const HotkeyRule& rule);
    HotkeyRule rule() const;

private slots:
    void browseTarget();
    void checkHotkeyOccupancy();
    void updateSingleInstanceAvailability();
    void applyManualHotkey();
    void updateManualHotkeyPreview();

private:
    QString uiText(UiText::Key key) const;
    void updateUiForCategory();
    LaunchActionType actionTypeForCategory() const;
    LaunchWindowState selectedWindowState() const;
    void setSelectedWindowState(LaunchWindowState windowState);
    HotkeyCombination manualHotkey() const;
    void setManualHotkey(const HotkeyCombination& hotkey);

    HotkeyRule m_rule;
    QString m_language = "zh-CN";
    LauncherCategory m_category = LauncherCategory::Program;
    QString m_sectionId;
    QCheckBox* m_ctrlCheck = nullptr;
    QCheckBox* m_altCheck = nullptr;
    QCheckBox* m_shiftCheck = nullptr;
    QCheckBox* m_winCheck = nullptr;
    QLineEdit* m_hotkeyKeyEdit = nullptr;
    QLineEdit* m_targetEdit = nullptr;
    QLineEdit* m_argumentsEdit = nullptr;
    QLineEdit* m_workingDirectoryEdit = nullptr;
    QLineEdit* m_descriptionEdit = nullptr;
    QComboBox* m_windowStateCombo = nullptr;
    QCheckBox* m_singleInstanceCheck = nullptr;
    QPushButton* m_browseButton = nullptr;
    QWidget* m_argumentsRow = nullptr;
    QWidget* m_workingDirectoryRow = nullptr;
    QWidget* m_singleInstanceRow = nullptr;
};
