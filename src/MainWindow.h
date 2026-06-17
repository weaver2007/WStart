#pragma once

#include "ActionRunner.h"
#include "HotkeyHookService.h"
#include "RuleStore.h"
#include "UiText.h"

#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPoint>
#include <QRect>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    QString language() const;
    bool hotkeysEnabled() const;

public slots:
    void showSettings();
    void setHotkeysPaused(bool paused);
    void applyHotkeysEnabled(bool enabled);

signals:
    void languageChanged(const QString &language);
    void hotkeysEnabledChanged(bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onHotkeyTriggered(const HotkeyRule &rule);

private:
    enum class ResizeRegion {
        None,
        Left,
        Right,
        Bottom,
        BottomLeft,
        BottomRight
    };

    void buildUi();
    void buildSettingsMenu();
    void retranslateUi();
    void rebuildNavItems();
    void loadDocument();
    void saveDocument();
    void saveDocumentSilently();
    void refreshHooks();
    void refreshLauncher();
    void rebuildSections();
    void updateLauncherGrids();
    void setCurrentCategory(LauncherCategory category);
    void setLanguage(const QString &language);
    void setThemeMode(const QString &themeMode);
    void applyTheme();
    void showItemAppearanceDialog();
    void showSectionAppearanceDialog();
    void showCategoryAppearanceDialog();
    void applyItemAppearanceChange();
    void applySectionAppearanceChange();
    void applyCategoryAppearanceChange();
    void applyCategoryAppearance();
    int fixedLauncherWidth() const;
    void applyFixedLauncherWidth();
    bool effectiveDarkTheme() const;
    void upsertRule(const HotkeyRule &rule);
    void setStatus(const QString &message);
    void showSectionMenu(const QString &sectionId, const QPoint &globalPos);
    void showListMenu(const QString &sectionId, QListWidget *list, const QPoint &viewportPos);
    void addSection(LauncherCategory category);
    void editSection(const QString &sectionId);
    void deleteSection(const QString &sectionId);
    void encryptSection(const QString &sectionId);
    void expandSectionOnly(const QString &sectionId);
    void toggleSectionCollapsed(const QString &sectionId);
    void enablePointerTracking(QWidget *widget);
    void addRuleToSection(const QString &sectionId);
    void editRule(const QString &ruleId);
    void deleteRule(const QString &ruleId);
    void runRule(const QString &ruleId);
    bool ensureSectionUnlocked(const QString &sectionId);
    bool isSectionUnlocked(const LauncherSection &section) const;
    int sectionIndexById(const QString &sectionId) const;
    int ruleIndexById(const QString &ruleId) const;
    QIcon iconForRule(const HotkeyRule &rule) const;
    QIcon iconForCategory(LauncherCategory category) const;
    QIcon iconForSection(const LauncherSection &section) const;
    QIcon themedIcon(const QString &key) const;
    bool rulePassesFilters(const HotkeyRule &rule) const;
    QString ruleTitle(const HotkeyRule &rule) const;
    QString categoryDisplayName(LauncherCategory category) const;
    QString passwordHash(const QString &password) const;
    QString uiText(UiText::Key key) const;
    ResizeRegion resizeRegionAt(const QPoint &position) const;
    void updateResizeCursor(const QPoint &position);
    void performResize(const QPoint &globalPosition);
    void finishInteractiveMove();
    void snapToTopIfNeeded();
    void setTopAutoHidden(bool hidden);
    void revealFromTopAutoHide();
    void updateTopAutoHide();
    void setAlwaysOnTop(bool enabled);
    QScreen *currentScreen() const;
    QRect currentScreenAvailableGeometry() const;

    RuleStore m_store;
    ActionRunner m_runner;
    HotkeyHookService m_hookService;
    LauncherDocument m_document;
    LauncherCategory m_currentCategory = LauncherCategory::Program;
    QSet<QString> m_unlockedSectionIds;
    QHash<QString, QListWidget *> m_sectionLists;
    QWidget *m_navBar = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QHash<LauncherCategory, QToolButton *> m_navButtons;
    QLineEdit *m_searchEdit = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_sectionsContainer = nullptr;
    QVBoxLayout *m_sectionsLayout = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_ruleCountLabel = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QToolButton *m_minButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QAction *m_hotkeysEnabledAction = nullptr;
    QAction *m_chineseAction = nullptr;
    QAction *m_englishAction = nullptr;
    QAction *m_themeSystemAction = nullptr;
    QAction *m_themeLightAction = nullptr;
    QAction *m_themeDarkAction = nullptr;
    QAction *m_itemAppearanceAction = nullptr;
    QAction *m_sectionAppearanceAction = nullptr;
    QAction *m_categoryAppearanceAction = nullptr;
    QPoint m_dragPosition;
    QPoint m_resizeStartGlobal;
    QRect m_resizeStartGeometry;
    ResizeRegion m_resizeRegion = ResizeRegion::None;
    bool m_resizing = false;
    QTimer m_autoHideTimer;
    QRect m_autoHideShownGeometry;
    bool m_topAutoHidden = false;
};
