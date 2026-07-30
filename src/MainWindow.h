#pragma once

#include "ActionRunner.h"
#include "HotkeyHookService.h"
#include "LauncherWindowInterface.h"
#include "RuleStore.h"
#include "UiText.h"
#include "UpdateChecker.h"

#include <QButtonGroup>
#include <QByteArray>
#include <QCheckBox>
#include <QFontComboBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

class QProgressDialog;
class QThread;

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class MainWindow : public QMainWindow, public LauncherWindowInterface {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    QString language() const override;
    bool hotkeysEnabled() const override;

public slots:
    void showSettings();
    void showStartupMinimized();
    void setHotkeysPaused(bool paused);
    void applyHotkeysEnabled(bool enabled);

signals:
    void languageChanged(const QString& language);
    void hotkeysEnabledChanged(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    bool winEvent(MSG* message, long* result);
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#else
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
#endif
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onHotkeyTriggered(const HotkeyRule& rule);
    void showSettingsMenu();
    void onCategoryButtonClicked(int id);
    void setLanguageChinese();
    void setLanguageEnglish();
    void setThemeSystem();
    void setThemeLight();
    void setThemeDark();
    void setPathStorageRelative();
    void setPathStorageAbsolute();
    void setUpdatesEnabled(bool enabled);
    void setStartupEnabled(bool enabled);
    void onStartupRegistrationFinished(bool enabled, bool success, QString error, bool userInitiated);
    void configureGithubToken();
    void checkForUpdates();
    void showAboutDialog();
    void onUpdateCheckFinished(bool updateAvailable, QString latestVersion, QString downloadUrl, QString releaseNotes,
                               QString error, bool silent);
    void onUpdateInfoReady(UpdateInfo info, QString error, bool silent);
    void onUpdateDownloadProgress(qint64 received, qint64 total);
    void onUpdateDownloadFinished(QString filePath, UpdateAsset asset, QString error);
    void showSectionContextMenu(const QPoint& pos);
    void showListContextMenu(const QPoint& pos);
    void runClickedRule(QListWidgetItem* item);
    void clearDragLaunchSuppression();
    void applyItemAppearanceDialogChange();
    void applySectionAppearanceDialogChange();
    void applyCategoryAppearanceDialogChange();
    void chooseSectionTextColor();
    void resetSectionTextColor();
    void chooseCategoryTextColor();
    void resetCategoryTextColor();
    void updateTopAutoHide();
    void setStatus(const QString& message);
    void refreshLauncher();
    void updateLauncherGrids();
    void showHotkeyListDialog();
    void hideHotkeyVisualFeedback();
    void showItemAppearanceDialog();
    void showSectionAppearanceDialog();
    void showCategoryAppearanceDialog();

private:
    enum class ResizeRegion { None, Left, Right, Bottom, BottomLeft, BottomRight };

    enum class PendingContextMenuKind { None, Section, List };

    void buildUi();
    void buildSettingsMenu();
    void retranslateUi();
    void rebuildNavItems();
    void loadDocument();
    bool saveDocument();
    bool saveDocumentSilently();
    void refreshHooks();
    void rebuildSections();
    void setCurrentCategory(LauncherCategory category);
    void setLanguage(const QString& language);
    void setThemeMode(const QString& themeMode);
    void setPathStorageMode(const QString& mode);
    void requestStartupRegistration(bool enabled, bool userInitiated);
    void applyTheme();
    void applyItemAppearanceChange();
    void applySectionAppearanceChange();
    void applyCategoryAppearanceChange();
    void applyCategoryAppearance();
    int fixedLauncherWidth() const;
    void applyFixedLauncherWidth();
    bool effectiveDarkTheme() const;
    void upsertRule(const HotkeyRule& rule);
    void showSectionMenu(const QString& sectionId, const QPoint& globalPos);
    void showListMenu(const QString& sectionId, QListWidget* list, const QPoint& viewportPos);
    void addSection(LauncherCategory category);
    void editSection(const QString& sectionId);
    void deleteSection(const QString& sectionId);
    void encryptSection(const QString& sectionId);
    void expandSectionOnly(const QString& sectionId);
    void toggleSectionCollapsed(const QString& sectionId);
    void enablePointerTracking(QWidget* widget);
    void addRuleToSection(const QString& sectionId);
    void addDroppedPathsToSection(const QString& sectionId, const QList<QUrl>& urls);
    bool canMoveRuleToSection(const QString& ruleId, const QString& sectionId, bool allowSameSection) const;
    bool moveRuleToSection(const QString& ruleId, const QString& sectionId, const QString& beforeRuleId, bool copy);
    QString ruleIdBeforeDrop(QListWidget* list, const QPoint& viewportPos, const QString& draggedRuleId) const;
    QString sectionIdAtGlobalPosition(const QPoint& globalPos) const;
    QString fallbackDropSectionId() const;
    void editRule(const QString& ruleId);
    void deleteRule(const QString& ruleId);
    void copyRuleToClipboard(const QString& ruleId, bool cut);
    bool canPasteRuleToSection(const QString& sectionId) const;
    void pasteRuleToSection(const QString& sectionId);
    void runRule(const QString& ruleId);
    void runRuleAsAdmin(const QString& ruleId);
    bool confirmDangerousRule(const HotkeyRule& rule);
    void showExplorerContextMenuForRule(const QString& ruleId, const QPoint& globalPos);
    void browseRuleTarget(const QString& ruleId);
    void createDesktopShortcutForRule(const QString& ruleId);
    void setRuleStartupShortcut(const QString& ruleId);
    void resetPendingContextMenu();
    bool ensureSectionUnlocked(const QString& sectionId);
    bool isSectionUnlocked(const LauncherSection& section) const;
    int sectionIndexById(const QString& sectionId) const;
    int ruleIndexById(const QString& ruleId) const;
    QIcon iconForRule(const HotkeyRule& rule) const;
    QIcon iconForCategory(LauncherCategory category) const;
    QIcon iconForSection(const LauncherSection& section) const;
    QIcon themedIcon(const QString& key) const;
    bool rulePassesFilters(const HotkeyRule& rule) const;
    QString ruleTitle(const HotkeyRule& rule) const;
    QString categoryDisplayName(LauncherCategory category) const;
    QString passwordHash(const QString& password) const;
    QString uiText(UiText::Key key) const;
    ResizeRegion resizeRegionAt(const QPoint& position) const;
    void updateResizeCursor(const QPoint& position);
    void performResize(const QPoint& globalPosition);
    void finishInteractiveMove();
    void snapToTopIfNeeded();
    void setTopAutoHidden(bool hidden);
    void revealFromTopAutoHide();
    void setAlwaysOnTop(bool enabled);
    void setTaskbarButtonVisible(bool visible);
    void showHotkeyVisualFeedback();
    QScreen* currentScreen() const;
    QRect currentScreenAvailableGeometry() const;

    RuleStore m_store;
    ActionRunner m_runner;
    HotkeyHookService m_hookService;
    UpdateChecker m_updateChecker;
    LauncherDocument m_document;
    LauncherDocument m_persistedDocument;
    QThread* m_startupRegistrationThread = nullptr;
    LauncherCategory m_currentCategory = LauncherCategory::Program;
    QSet<QString> m_unlockedSectionIds;
    QHash<QString, QListWidget*> m_sectionLists;
    QWidget* m_navBar = nullptr;
    QButtonGroup* m_navGroup = nullptr;
    QHash<LauncherCategory, QToolButton*> m_navButtons;
    QLineEdit* m_searchEdit = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_sectionsContainer = nullptr;
    QVBoxLayout* m_sectionsLayout = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_ruleCountLabel = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QToolButton* m_minButton = nullptr;
    QToolButton* m_closeButton = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_languageMenu = nullptr;
    QMenu* m_themeMenu = nullptr;
    QMenu* m_pathStorageMenu = nullptr;
    QAction* m_hotkeysEnabledAction = nullptr;
    QAction* m_updatesEnabledAction = nullptr;
    QAction* m_startupEnabledAction = nullptr;
    QAction* m_githubTokenAction = nullptr;
    QAction* m_checkUpdatesAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_chineseAction = nullptr;
    QAction* m_englishAction = nullptr;
    QAction* m_themeSystemAction = nullptr;
    QAction* m_themeLightAction = nullptr;
    QAction* m_themeDarkAction = nullptr;
    QAction* m_pathRelativeAction = nullptr;
    QAction* m_pathAbsoluteAction = nullptr;
    QAction* m_itemAppearanceAction = nullptr;
    QAction* m_sectionAppearanceAction = nullptr;
    QAction* m_categoryAppearanceAction = nullptr;
    QAction* m_hotkeyListAction = nullptr;
    QSpinBox* m_itemIconWidthSpin = nullptr;
    QSpinBox* m_itemIconHeightSpin = nullptr;
    QSpinBox* m_itemWidthSpin = nullptr;
    QSpinBox* m_itemHeightSpin = nullptr;
    QSpinBox* m_itemFontPointSizeSpin = nullptr;
    QSpinBox* m_itemHorizontalSpacingSpin = nullptr;
    QSpinBox* m_itemVerticalSpacingSpin = nullptr;
    QFontComboBox* m_itemFontFamilyCombo = nullptr;
    QCheckBox* m_itemMultilineCheck = nullptr;
    QCheckBox* m_itemEllipsisCheck = nullptr;
    QSpinBox* m_sectionIconWidthSpin = nullptr;
    QSpinBox* m_sectionIconHeightSpin = nullptr;
    QSpinBox* m_sectionHeaderHeightSpin = nullptr;
    QSpinBox* m_sectionFontPointSizeSpin = nullptr;
    QFontComboBox* m_sectionFontFamilyCombo = nullptr;
    QPushButton* m_sectionColorButton = nullptr;
    QSpinBox* m_categoryIconWidthSpin = nullptr;
    QSpinBox* m_categoryIconHeightSpin = nullptr;
    QSpinBox* m_categoryButtonHeightSpin = nullptr;
    QSpinBox* m_categoryFontPointSizeSpin = nullptr;
    QFontComboBox* m_categoryFontFamilyCombo = nullptr;
    QPushButton* m_categoryColorButton = nullptr;
    QPoint m_dragPosition;
    QPoint m_resizeStartGlobal;
    QRect m_resizeStartGeometry;
    ResizeRegion m_resizeRegion = ResizeRegion::None;
    bool m_resizing = false;
    PendingContextMenuKind m_pendingContextMenuKind = PendingContextMenuKind::None;
    QPointer<QListWidget> m_pendingContextList;
    QString m_pendingContextSectionId;
    QPoint m_pendingContextViewportPos;
    QPoint m_pendingContextGlobalPos;
    bool m_ignoreNextContextMenuEvent = false;
    QTimer m_autoHideTimer;
    QPointer<QWidget> m_hotkeyFeedbackOverlay;
    QRect m_autoHideShownGeometry;
    bool m_topAutoHidden = false;
    QPointer<QProgressDialog> m_updateProgressDialog;
    UpdateInfo m_pendingUpdateInfo;
    HotkeyRule m_ruleClipboard;
    QString m_ruleClipboardSourceRuleId;
    bool m_hasRuleClipboard = false;
    bool m_ruleClipboardCut = false;
    bool m_ignoreNextLeftReleaseAfterDrag = false;
};
