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
    Q_PROPERTY(bool showProgressFrame READ GetShowProgressFrame NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showInstallationStatusBadge READ GetShowInstallationStatusBadge NOTIFY signalConfigChanged)
    Q_PROPERTY(bool progressFrameGrayscaleMode READ GetProgressFrameGrayscaleMode NOTIFY signalConfigChanged)
    Q_PROPERTY(bool showTotalAchievementsBadge READ GetShowTotalAchievementsBadge NOTIFY signalConfigChanged)
    Q_PROPERTY(bool enableProgressFrameCompletionAnimation READ GetEnableProgressFrameCompletionAnimation NOTIFY signalConfigChanged)
    Q_PROPERTY(bool enableDynamicAchievementRows READ GetEnableDynamicAchievementRows NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeX READ GetWindowSizeX NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeY READ GetWindowSizeY NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeXDefault READ GetWindowSizeXDefault NOTIFY signalConfigChanged)
    Q_PROPERTY(uint16_t windowSizeYDefault READ GetWindowSizeYDefault NOTIFY signalConfigChanged)
    Q_PROPERTY(QString steamId READ GetSteamId NOTIFY signalConfigChanged)
    Q_PROPERTY(QString steamWebApiKey READ GetSteamWebApiKey NOTIFY signalConfigChanged)
    Q_PROPERTY(bool backendService READ GetBackendService NOTIFY signalConfigChanged)

public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings();

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
        ShowProgressFrame,
        ShowInstallationStatusBadge,
        ProgressFrameGrayscaleMode,
        ShowTotalAchievementsBadge,
        EnableProgressFrameCompletionAnimation,
        EnableDynamicAchievementRows,
        WindowSizeX,
        WindowSizeY,
        SteamId,
        SteamWebApiKey,
        BackendService
    };
    Q_ENUM(Key)

    Q_INVOKABLE void SetPinCode(const QString &pinCode);
    Q_INVOKABLE bool ResetDefaults();
    Q_INVOKABLE bool SaveConfig();
    Q_INVOKABLE bool LoadConfig();
    Q_INVOKABLE bool SaveValue(Key key, const QVariant &value, bool emitSignal = true);
    void TrackWindowSizeSetting(QQmlApplicationEngine *engine);

    inline QString GetTheme() const { return m_theme; }
    inline bool GetShowLymalinkLogo() const { return m_showLymalinkLogo; }
    inline QString GetLanguage() const { return m_language; }
    inline bool GetCloseToTray() const { return m_closeToTray; }
    inline bool GetCloseToTrayToast() const { return m_closeToTrayToast; }
    inline bool GetShowCollapseButton() const { return m_showCollapseButton; }
    inline bool GetShowTooltips() const { return m_showTooltips; }
    inline bool GetEnableCollapseBorderButton() const { return m_enableCollapseBorderButton; }
    inline bool GetShowProgressFrame() const { return m_showProgressFrame; }
    inline bool GetShowInstallationStatusBadge() const { return m_showInstallationStatusBadge; }
    inline bool GetProgressFrameGrayscaleMode() const { return m_progressFrameGrayscaleMode; }
    inline bool GetShowTotalAchievementsBadge() const { return m_showTotalAchievementsBadge; }
    inline bool GetEnableProgressFrameCompletionAnimation() const { return m_enableProgressFrameCompletionAnimation; }
    inline bool GetEnableDynamicAchievementRows() const { return m_enableDynamicAchievementRows; }
    inline uint16_t GetWindowSizeX() const { return m_windowSizeX; }
    inline uint16_t GetWindowSizeY() const { return m_windowSizeY; }
    inline uint16_t GetWindowSizeXDefault() const { return m_windowSizeXDefault; }
    inline uint16_t GetWindowSizeYDefault() const { return m_windowSizeYDefault; }
    inline QString GetSteamId() const { return m_steamId; }
    inline QString GetSteamWebApiKey() const { return m_steamWebApiKey; }
    inline bool GetBackendService() const { return m_backendService; }

signals:
    void signalConfigChanged();

private:
    QSettings *m_settings;
    QString m_pinCode;

    QString m_theme;
    bool m_showLymalinkLogo;
    QString m_language;
    bool m_closeToTray;
    bool m_closeToTrayToast;
    bool m_showCollapseButton;
    bool m_showTooltips;
    bool m_enableCollapseBorderButton;
    bool m_showProgressFrame;
    bool m_showInstallationStatusBadge;
    bool m_progressFrameGrayscaleMode;
    bool m_showTotalAchievementsBadge;
    bool m_enableProgressFrameCompletionAnimation;
    bool m_enableDynamicAchievementRows;
    uint16_t m_windowSizeX;
    uint16_t m_windowSizeY;
    uint16_t m_windowSizeXDefault;
    uint16_t m_windowSizeYDefault;
    QString m_steamId;
    QString m_steamWebApiKey;
    bool m_backendService;

    void SetDefaults();
    bool SaveSteamWebApiKey();
    void SavePlainValues();
    bool SaveEncryptedValues();
    void LoadEncryptedValues();
};
