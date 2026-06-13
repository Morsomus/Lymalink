/////////////////////////////////////////////////////////
// File: Settings.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Settings
/////////////////////////////////////////////////////////

#pragma once

#include "Defines.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QQmlApplicationEngine>

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ GetTheme NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showLymalinkLogo READ GetShowLymalinkLogo NOTIFY signalConfigChanged)
    Q_PROPERTY(QString language READ GetLanguage NOTIFY signalConfigChanged)
    Q_PROPERTY(bool closeToTray READ GetCloseToTray NOTIFY signalConfigChanged)
    Q_PROPERTY(bool closeToTrayToast READ GetCloseToTrayToast NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showCollapseButton READ GetShowCollapseButton NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showTooltips READ GetShowTooltips NOTIFY signalConfigChanged)
    Q_PROPERTY(bool enableCollapseBorderButton READ GetEnableCollapseBorderButton NOTIFY signalConfigChanged)
    Q_PROPERTY(int globalColorStyle READ GetGlobalColorStyle NOTIFY signalConfigChanged)
    Q_PROPERTY(int progressFrameColorStyle READ GetProgressFrameColorStyle NOTIFY signalConfigChanged)
    Q_PROPERTY(int progressBarColorStyle READ GetProgressBarColorStyle NOTIFY signalConfigChanged)
    Q_PROPERTY(int targetTypeBadgeColorStyle READ GetTargetTypeBadgeColorStyle NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showInstallationStatusBadge READ GetShowInstallationStatusBadge NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showTotalAchievementsBadge READ GetShowTotalAchievementsBadge NOTIFY signalConfigChanged)
    Q_PROPERTY(bool enableProgressFrameCompletionAnimation READ GetEnableProgressFrameCompletionAnimation NOTIFY signalConfigChanged)
    Q_PROPERTY(bool enableDynamicAchievementRows READ GetEnableDynamicAchievementRows NOTIFY signalConfigChanged)
    Q_PROPERTY(bool sidebarCollapsed READ GetSidebarCollapsed NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeX READ GetWindowSizeX NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeY READ GetWindowSizeY NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeXDefault READ GetWindowSizeXDefault NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeYDefault READ GetWindowSizeYDefault NOTIFY signalConfigChanged)
    Q_PROPERTY(QString steamId READ GetSteamId NOTIFY signalConfigChanged)
    Q_PROPERTY(QString steamWebApiKey READ GetSteamWebApiKey NOTIFY signalConfigChanged)
    Q_PROPERTY(QString notificationSound READ GetNotificationSound NOTIFY signalConfigChanged)
    Q_PROPERTY(QStringList notificationSounds READ GetNotificationSounds NOTIFY signalConfigChanged)
    Q_PROPERTY(QString overlayNotificationPosition READ GetOverlayNotificationPosition NOTIFY signalConfigChanged)
    Q_PROPERTY(bool startupNotification READ GetStartupNotification NOTIFY signalConfigChanged)
    Q_PROPERTY(bool customNotificationSound READ GetCustomNotificationSound NOTIFY signalConfigChanged)
    Q_PROPERTY(QString customNotificationSoundPath READ GetCustomNotificationSoundPath NOTIFY signalConfigChanged)
    Q_PROPERTY(QString dashboardToolbarSort READ GetDashboardToolbarSort NOTIFY signalConfigChanged)
    Q_PROPERTY(QStringList dashboardToolbarFilters READ GetDashboardToolbarFilters NOTIFY signalConfigChanged)
    Q_PROPERTY(bool dashboardToolbarSortDescending READ GetDashboardToolbarSortDescending NOTIFY signalConfigChanged)
    Q_PROPERTY(QString dashboardToolbarLayout READ GetDashboardToolbarLayout NOTIFY signalConfigChanged)
    Q_PROPERTY(QString currentVersion READ GetCurrentVersion NOTIFY signalConfigChanged)
    Q_PROPERTY(QString welcomeHelpText READ GetWelcomeHelpText NOTIFY signalConfigChanged)
    Q_PROPERTY(QString targetDetailsHelpText READ GetTargetDetailsHelpText NOTIFY signalConfigChanged)

public:
    enum Key
    {
        Theme,
        ShowLymalinkLogo,
        Language,
        CloseToTray,
        CloseToTrayToast,
        ShowCollapseButton,
        ShowTooltips,
        EnableCollapseBorderButton,
        GlobalColorStyle,
        ProgressFrameColorStyle,
        ProgressBarColorStyle,
        ShowInstallationStatusBadge,
        ShowTotalAchievementsBadge,
        TargetTypeBadgeColorStyle,
        EnableProgressFrameCompletionAnimation,
        EnableDynamicAchievementRows,
        SidebarCollapsed,
        WindowSizeX,
        WindowSizeY,
        SteamId,
        SteamWebApiKey,
        NotificationSound,
        OverlayNotificationPosition,
        StartupNotification,
        CustomNotificationSound,
        CustomNotificationSoundPath,
        DashboardToolbarSort,
        DashboardToolbarFilters,
        DashboardToolbarSortDescending,
        DashboardToolbarLayout,
        WelcomeHelpText,
        TargetDetailsHelpText
    };
    Q_ENUM(Key)

    explicit Settings(QObject *parent = nullptr);
    ~Settings();

    void TrackWindowSizeSetting(QQmlApplicationEngine *engine);

    Q_INVOKABLE void SetTempEncryptionKey(const QString &encryptionKey);
    Q_INVOKABLE QString GetSteamWebApiKeyPlain() const;
    Q_INVOKABLE bool ResetDefaults();
    Q_INVOKABLE bool SaveConfig();
    Q_INVOKABLE bool LoadConfig();
    Q_INVOKABLE bool SaveValue(Key key, const QVariant &value, bool emitSignal = true);
    Q_INVOKABLE QString GetConfigFilePath() const;

    inline QString GetTheme() const { return m_theme; }
    inline bool GetShowLymalinkLogo() const { return m_showLymalinkLogo; }
    inline QString GetLanguage() const { return m_language; }
    inline bool GetCloseToTray() const { return m_closeToTray; }
    inline bool GetCloseToTrayToast() const { return m_closeToTrayToast; }
    inline bool GetShowCollapseButton() const { return m_showCollapseButton; }
    inline bool GetShowTooltips() const { return m_showTooltips; }
    inline bool GetEnableCollapseBorderButton() const { return m_enableCollapseBorderButton; }
    inline bool GetSidebarCollapsed() const { return m_sidebarCollapsed; }
    inline int GetGlobalColorStyle() const { return m_globalColorStyle; }
    inline int GetProgressFrameColorStyle() const { return m_progressFrameColorStyle; }
    inline int GetProgressBarColorStyle() const { return m_progressBarColorStyle; }
    inline int GetTargetTypeBadgeColorStyle() const { return m_targetTypeBadgeColorStyle; }
    inline bool GetShowInstallationStatusBadge() const { return m_showInstallationStatusBadge; }
    inline bool GetShowTotalAchievementsBadge() const { return m_showTotalAchievementsBadge; }
    inline bool GetEnableProgressFrameCompletionAnimation() const { return m_enableProgressFrameCompletionAnimation; }
    inline bool GetEnableDynamicAchievementRows() const { return m_enableDynamicAchievementRows; }
    inline uint16_t GetWindowSizeX() const { return m_windowSizeX; }
    inline uint16_t GetWindowSizeY() const { return m_windowSizeY; }
    inline uint16_t GetWindowSizeXDefault() const { return m_windowSizeXDefault; }
    inline uint16_t GetWindowSizeYDefault() const { return m_windowSizeYDefault; }
    inline QString GetSteamId() const { return m_steamId; }
    inline QString GetSteamWebApiKey() const { return m_steamWebApiKey; }
    inline QString GetNotificationSound() const { return m_notificationSound; }
    QStringList GetNotificationSounds() const;
    inline QString GetOverlayNotificationPosition() const { return m_overlayNotificationPosition; }
    inline bool GetStartupNotification() const { return m_startupNotification; }
    inline bool GetCustomNotificationSound() const { return m_customNotificationSound; }
    inline QString GetCustomNotificationSoundPath() const { return m_customNotificationSoundPath; }
    inline QString GetDashboardToolbarSort() const { return m_dashboardToolbarSort; }
    inline QStringList GetDashboardToolbarFilters() const { return m_dashboardToolbarFilters; }
    inline bool GetDashboardToolbarSortDescending() const { return m_dashboardToolbarSortDescending; }
    inline QString GetDashboardToolbarLayout() const { return m_dashboardToolbarLayout; }
    inline QString GetCurrentVersion() const { return m_currentVersion; }
    inline QString GetWelcomeHelpText() const { return m_welcomeHelpText; }
    inline QString GetTargetDetailsHelpText() const { return m_targetDetailsHelpText; }

signals:
    void signalConfigChanged();
    void signalDefaultsReset();

private:
    QSettings m_settings;
    QString m_tempEncryptionKey;

    QString m_theme;
    bool m_showLymalinkLogo;
    QString m_language;
    bool m_closeToTray;
    bool m_closeToTrayToast;
    bool m_showCollapseButton;
    bool m_showTooltips;
    bool m_enableCollapseBorderButton;
    int m_globalColorStyle;
    int m_progressFrameColorStyle;
    int m_progressBarColorStyle;
    int m_targetTypeBadgeColorStyle;
    bool m_showInstallationStatusBadge;
    bool m_showTotalAchievementsBadge;
    bool m_enableProgressFrameCompletionAnimation;
    bool m_enableDynamicAchievementRows;
    bool m_sidebarCollapsed;
    uint16_t m_windowSizeX;
    uint16_t m_windowSizeY;
    uint16_t m_windowSizeXDefault;
    uint16_t m_windowSizeYDefault;
    QString m_steamId;
    QString m_steamWebApiKey;
    QString m_notificationSound;
    QString m_overlayNotificationPosition;
    bool m_startupNotification;
    bool m_customNotificationSound;
    QString m_customNotificationSoundPath;
    QString m_dashboardToolbarSort;
    QStringList m_dashboardToolbarFilters;
    bool m_dashboardToolbarSortDescending;
    QString m_dashboardToolbarLayout;
    QString m_currentVersion;
    QString m_welcomeHelpText;
    QString m_targetDetailsHelpText;

    void SetDefaults();
    QString ResolveDefaultNotificationSound() const;
    bool SaveSteamWebApiKey(const QString &webApiKey);
    void SavePlainValues();
    bool SaveEncryptedWebApiKey(const QString &webApiKey);
    void LoadEncryptedValueState();
    QString NormalizeTheme(const QString &theme);
};
