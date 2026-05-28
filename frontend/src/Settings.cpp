/////////////////////////////////////////////////////////
// File: Settings.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Settings
/////////////////////////////////////////////////////////

#include "Settings.h"
#include "tools/Encryption.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

/////////////////////////////////////////////////////////////////////

Settings::Settings(QObject *parent) : QObject(parent),
    m_settings(QSettings::IniFormat, QSettings::UserScope, ORGANIZATION, APPLICATION)
{
    m_tempEncryptionKey = "";
    m_windowSizeXDefault = 1320;
    m_windowSizeYDefault = 900;
    SetDefaults();

    if (!QFileInfo::exists(m_settings.fileName()))
    {
        SaveConfig();
    }
    else
    {
        LoadConfig();
    }
}

Settings::~Settings()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void Settings::TrackWindowSizeSetting(QQmlApplicationEngine *engine)
{
    // Window size tracking
    const QList<QObject*> rootObjects = engine->rootObjects();
    if (!rootObjects.isEmpty()) {
        QWindow *window = qobject_cast<QWindow *>(rootObjects.first());

        if (window != nullptr) {
            QTimer *resizeTimer = new QTimer(window);
            resizeTimer->setSingleShot(true);
            resizeTimer->setInterval(1000);

            QObject::connect(window, &QWindow::widthChanged, resizeTimer, qOverload<>(&QTimer::start));
            QObject::connect(window, &QWindow::heightChanged, resizeTimer, qOverload<>(&QTimer::start));
            QObject::connect(resizeTimer, &QTimer::timeout, window, [this, window]() {
                qDebug() << "Window Size:" << window->width() << window->height();
                SaveValue(WindowSizeX, window->width(), false);
                SaveValue(WindowSizeY, window->height());
            });
        }
    }
}

/////////////////////////////////////////////////////////////////////

void Settings::SetTempEncryptionKey(const QString &encryptionKey)
{
    m_tempEncryptionKey = encryptionKey;
}

/////////////////////////////////////////////////////////////////////

bool Settings::ResetDefaults()
{
    qDebug() << "Settings::ResetDefaults - Resetting configuration to defaults...";

    SetDefaults();
    m_settings.clear();

    return SaveConfig();
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveConfig()
{
    qDebug() << "Settings::SaveConfig - Saving configuration...";

    SavePlainValues();

    m_settings.sync();

    const bool saved = (m_settings.status() == QSettings::NoError);
    if (saved)
    {
        emit signalConfigChanged();
    }

    return saved;
}

/////////////////////////////////////////////////////////////////////

bool Settings::LoadConfig()
{
    qDebug() << "Settings::LoadConfig - Loading configuration from: " << m_settings.fileName();

    m_settings.beginGroup(GROUP_APPEARANCE);
    m_theme = m_settings.value("Theme", m_theme).toString();
    m_showLymalinkLogo = m_settings.value("ShowLymalinkLogo", m_showLymalinkLogo).toBool();
    m_language = m_settings.value("Language", m_language).toString();
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_INTERFACE);
    m_closeToTray = m_settings.value("CloseToTray", m_closeToTray).toBool();
    m_closeToTrayToast = m_settings.value("CloseToTrayToast", m_closeToTrayToast).toBool();
    m_showCollapseButton = m_settings.value("ShowCollapseButton", m_showCollapseButton).toBool();
    m_showTooltips = m_settings.value("ShowTooltips", m_showTooltips).toBool();
    m_enableCollapseBorderButton = m_settings.value("EnableCollapseBorderButton", m_enableCollapseBorderButton).toBool();
    m_sidebarCollapsed = m_settings.value("SidebarCollapsed", m_sidebarCollapsed).toBool();
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_DISPLAY);
    m_globalColorStyle = m_settings.value("GlobalColorStyle", m_globalColorStyle).toInt();
    m_progressFrameColorStyle = m_settings.value("ProgressFrameColorStyle", m_progressFrameColorStyle).toInt();
    m_progressBarColorStyle = m_settings.value("ProgressBarColorStyle", m_progressBarColorStyle).toInt();
    m_showProgressFrame = m_settings.value("ShowProgressFrame", m_showProgressFrame).toBool();
    m_showProgressBar = m_settings.value("ShowProgressBar", m_showProgressBar).toBool();
    m_showInstallationStatusBadge = m_settings.value("ShowInstallationStatusBadge", m_showInstallationStatusBadge).toBool();
    m_progressFrameGrayscaleMode = m_settings.value("ProgressFrameGrayscaleMode", m_progressFrameGrayscaleMode).toBool();
    m_showTotalAchievementsBadge = m_settings.value("ShowTotalAchievementsBadge", m_showTotalAchievementsBadge).toBool();
    m_showTargetTypeBadge = m_settings.value("ShowTargetTypeBadge", m_showTargetTypeBadge).toBool();
    m_enableProgressFrameCompletionAnimation = m_settings.value("EnableProgressFrameCompletionAnimation", m_enableProgressFrameCompletionAnimation).toBool();
    m_enableDynamicAchievementRows = m_settings.value("EnableDynamicAchievementRows", m_enableDynamicAchievementRows).toBool();
    m_windowSizeX = m_settings.value("WindowSizeX", m_windowSizeX).toUInt();
    m_windowSizeY = m_settings.value("WindowSizeY", m_windowSizeY).toUInt();
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    m_steamId = m_settings.value("SteamId", m_steamId).toString();
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_BACKGROUND_SERVICE);
    m_notificationSound = m_settings.value("NotificationSound", m_notificationSound).toString();
    m_overlayNotificationPosition = m_settings.value("OverlayNotificationPosition", m_overlayNotificationPosition).toString();
    m_customNotificationSound = m_settings.value("CustomNotificationSound", m_customNotificationSound).toBool();
    m_customNotificationSoundPath = m_settings.value("CustomNotificationSoundPath", m_customNotificationSoundPath).toString();
    m_settings.endGroup();
    // Fallback if saved sound no longer available
    if (!GetNotificationSounds().contains(m_notificationSound))
    {
        m_notificationSound = ResolveDefaultNotificationSound();
    }

    m_settings.beginGroup(GROUP_DASHBOARD);
    m_dashboardToolbarSort = m_settings.value("ToolbarSort", m_dashboardToolbarSort).toString();
    m_dashboardToolbarFilters = m_settings.value("ToolbarFilters", m_dashboardToolbarFilters).toStringList();
    m_dashboardToolbarSortDescending = m_settings.value("ToolbarSortDescending", m_dashboardToolbarSortDescending).toBool();
    m_dashboardToolbarLayout = m_settings.value("ToolbarLayout", m_dashboardToolbarLayout).toString();
    m_welcomeHelpText = m_settings.value("WelcomeHelpText", m_welcomeHelpText).toBool();
    m_targetDetailsHelpText = m_settings.value("TargetDetailsHelpText", m_targetDetailsHelpText).toBool();
    m_settings.endGroup();

    LoadEncryptedValueState();

    emit signalConfigChanged();

    return (m_settings.status() == QSettings::NoError);
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveValue(Key key, const QVariant &value, bool emitSignal)
{

    qDebug() << "Settings::SaveValue - saving key:" << key;

    QString group;
    QString settingsKey;
    QVariant settingsValue;

    switch (key)
    {
        case Theme:
        {
            m_theme = value.toString();
            group = GROUP_APPEARANCE;
            settingsKey = "Theme";
            settingsValue = m_theme;
            break;
        }
        case ShowLymalinkLogo:
        {
            m_showLymalinkLogo = value.toBool();
            group = GROUP_APPEARANCE;
            settingsKey = "ShowLymalinkLogo";
            settingsValue = m_showLymalinkLogo;
            break;
        }   
        case Language:
        {
            m_language = value.toString();
            group = GROUP_APPEARANCE;
            settingsKey = "Language";
            settingsValue = m_language;
            break;
        }
        case CloseToTray:
        {
            m_closeToTray = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "CloseToTray";
            settingsValue = m_closeToTray;
            break;
        }  
        case CloseToTrayToast:
        {
            m_closeToTrayToast = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "CloseToTrayToast";
            settingsValue = m_closeToTrayToast;
            break;
        }
        case ShowCollapseButton:
        {
            m_showCollapseButton = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "ShowCollapseButton";
            settingsValue = m_showCollapseButton;
            break;
        }
        case ShowTooltips:
        {
            m_showTooltips = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "ShowTooltips";
            settingsValue = m_showTooltips;
            break;
        }
        case EnableCollapseBorderButton:
        {
            m_enableCollapseBorderButton = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "EnableCollapseBorderButton";
            settingsValue = m_enableCollapseBorderButton;
            break;
        }
        case SidebarCollapsed:
        {
            m_sidebarCollapsed = value.toBool();
            group = GROUP_INTERFACE;
            settingsKey = "SidebarCollapsed";
            settingsValue = m_sidebarCollapsed;
            break;
        }
        case GlobalColorStyle:
        {
            m_globalColorStyle = value.toInt();
            group = GROUP_DISPLAY;
            settingsKey = "GlobalColorStyle";
            settingsValue = m_globalColorStyle;
            break;
        }
        case ProgressFrameColorStyle:
        {
            m_progressFrameColorStyle = value.toInt();
            group = GROUP_DISPLAY;
            settingsKey = "ProgressFrameColorStyle";
            settingsValue = m_progressFrameColorStyle;
            break;
        }
        case ProgressBarColorStyle:
        {
            m_progressBarColorStyle = value.toInt();
            group = GROUP_DISPLAY;
            settingsKey = "ProgressBarColorStyle";
            settingsValue = m_progressBarColorStyle;
            break;
        }
        case ShowProgressFrame:
        {
            m_showProgressFrame = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowProgressFrame";
            settingsValue = m_showProgressFrame;
            break;
        }
        case ShowProgressBar:
        {
            m_showProgressBar = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowProgressBar";
            settingsValue = m_showProgressBar;
            break;
        }
        case ShowInstallationStatusBadge:
        {
            m_showInstallationStatusBadge = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowInstallationStatusBadge";
            settingsValue = m_showInstallationStatusBadge;
            break;
        }
        case ProgressFrameGrayscaleMode:
        {
            m_progressFrameGrayscaleMode = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ProgressFrameGrayscaleMode";
            settingsValue = m_progressFrameGrayscaleMode;
            break;
        }
        case ShowTotalAchievementsBadge:
        {
            m_showTotalAchievementsBadge = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowTotalAchievementsBadge";
            settingsValue = m_showTotalAchievementsBadge;
            break;
        }
        case ShowTargetTypeBadge:
        {
            m_showTargetTypeBadge = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowTargetTypeBadge";
            settingsValue = m_showTargetTypeBadge;
            break;
        }
        case EnableProgressFrameCompletionAnimation:
        {
            m_enableProgressFrameCompletionAnimation = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "EnableProgressFrameCompletionAnimation";
            settingsValue = m_enableProgressFrameCompletionAnimation;
            break;
        }
        case EnableDynamicAchievementRows:
        {
            m_enableDynamicAchievementRows = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "EnableDynamicAchievementRows";
            settingsValue = m_enableDynamicAchievementRows;
            break;
        }
        case WindowSizeX:
        {
            m_windowSizeX = value.toUInt();
            group = GROUP_DISPLAY;
            settingsKey = "WindowSizeX";
            settingsValue = m_windowSizeX;
            break;
        }
        case WindowSizeY:
        {
            m_windowSizeY = value.toUInt();
            group = GROUP_DISPLAY;
            settingsKey = "WindowSizeY";
            settingsValue = m_windowSizeY;
            break;
        }
        case SteamId:
        {
            m_steamId = value.toString();
            group = GROUP_STEAM_WEB_API;
            settingsKey = "SteamId";
            settingsValue = m_steamId;
            break;
        }
        case SteamWebApiKey:
        {
            const QString val = value.toString();
            const bool resetWebApiKey = val == "reset";
            if (resetWebApiKey)
            {
                m_steamWebApiKey = "";
                group = GROUP_STEAM_WEB_API;
                settingsKey = "WebApiKey";
                settingsValue = m_steamWebApiKey;
                break;
            }
            else
            {
                return SaveSteamWebApiKey(val);
            }
        }
        case NotificationSound:
        {
            m_notificationSound = value.toString();
            group = GROUP_BACKGROUND_SERVICE;
            settingsKey = "NotificationSound";
            settingsValue = m_notificationSound;
            break;
        }
        case OverlayNotificationPosition:
        {
            m_overlayNotificationPosition = value.toString();
            group = GROUP_BACKGROUND_SERVICE;
            settingsKey = "OverlayNotificationPosition";
            settingsValue = m_overlayNotificationPosition;
            break;
        }
        case CustomNotificationSound:
        {
            m_customNotificationSound = value.toBool();
            group = GROUP_BACKGROUND_SERVICE;
            settingsKey = "CustomNotificationSound";
            settingsValue = m_customNotificationSound;
            break;
        }
        case CustomNotificationSoundPath:
        {
            m_customNotificationSoundPath = value.toString();
            group = GROUP_BACKGROUND_SERVICE;
            settingsKey = "CustomNotificationSoundPath";
            settingsValue = m_customNotificationSoundPath;
            break;
        }
        case DashboardToolbarSort:
        {
            m_dashboardToolbarSort = value.toString();
            group = GROUP_DASHBOARD;
            settingsKey = "ToolbarSort";
            settingsValue = m_dashboardToolbarSort;
            break;
        }
        case DashboardToolbarFilters:
        {
            m_dashboardToolbarFilters = value.toStringList();
            group = GROUP_DASHBOARD;
            settingsKey = "ToolbarFilters";
            settingsValue = m_dashboardToolbarFilters;
            break;
        }
        case DashboardToolbarSortDescending:
        {
            m_dashboardToolbarSortDescending = value.toBool();
            group = GROUP_DASHBOARD;
            settingsKey = "ToolbarSortDescending";
            settingsValue = m_dashboardToolbarSortDescending;
            break;
        }
        case DashboardToolbarLayout:
        {
            m_dashboardToolbarLayout = value.toString();
            group = GROUP_DASHBOARD;
            settingsKey = "ToolbarLayout";
            settingsValue = m_dashboardToolbarLayout;
            break;
        }
        case WelcomeHelpText:
        {
            m_welcomeHelpText = value.toBool();
            group = GROUP_DASHBOARD;
            settingsKey = "WelcomeHelpText";
            settingsValue = m_welcomeHelpText;
            break;
        }
        case TargetDetailsHelpText:
        {
            m_targetDetailsHelpText = value.toBool();
            group = GROUP_DASHBOARD;
            settingsKey = "TargetDetailsHelpText";
            settingsValue = m_targetDetailsHelpText;
            break;
        }
        default:
        {
            qDebug() << "Settings::SaveValue - unknown setting key:" << key;
            return false;
        } 
    }

    m_settings.beginGroup(group);
    m_settings.setValue(settingsKey, settingsValue);
    m_settings.endGroup();

    m_settings.sync();

    const bool saved = (m_settings.status() == QSettings::NoError);
    if (emitSignal && saved)
    {
        emit signalConfigChanged();
    }

    return saved;
}

/////////////////////////////////////////////////////////////////////

QString Settings::GetConfigFilePath() const
{
    return m_settings.fileName();
}

/////////////////////////////////////////////////////////////////////

QStringList Settings::GetNotificationSounds() const
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir soundDir(dataRoot + "/Lymalink/sounds");
    return soundDir.entryList(QStringList{"*.ogg"}, QDir::Files | QDir::Readable, QDir::Name);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void Settings::SetDefaults()
{
    m_tempEncryptionKey.clear();
    m_theme = "dark";
    m_showLymalinkLogo = true;
    m_language = "English";
    m_closeToTray = true;
    m_closeToTrayToast = true;
    m_showCollapseButton = true;
    m_showTooltips = true;
    m_enableCollapseBorderButton = true;
    m_sidebarCollapsed = false;
    m_globalColorStyle = 1;
    m_progressFrameColorStyle = 1;
    m_progressBarColorStyle = 5;
    m_showProgressFrame = true;
    m_showProgressBar = true;
    m_showInstallationStatusBadge = true;
    m_progressFrameGrayscaleMode = false;
    m_showTotalAchievementsBadge = true;
    m_showTargetTypeBadge = false;
    m_enableProgressFrameCompletionAnimation = true;
    m_enableDynamicAchievementRows = true;
    m_windowSizeX = m_windowSizeXDefault;
    m_windowSizeY = m_windowSizeYDefault;
    m_steamId = "";
    m_steamWebApiKey = "";
    m_notificationSound = ResolveDefaultNotificationSound();
    m_overlayNotificationPosition = "bottom-right";
    m_customNotificationSound = false;
    m_customNotificationSoundPath = "";
    m_dashboardToolbarSort = "title";
    m_dashboardToolbarFilters = QStringList{"none"};
    m_dashboardToolbarSortDescending = false;
    m_dashboardToolbarLayout = "defaultCardGrid";
    m_welcomeHelpText = false;
    m_targetDetailsHelpText = false;
}

/////////////////////////////////////////////////////////////////////

QString Settings::ResolveDefaultNotificationSound() const
{
    const QStringList notificationSounds = GetNotificationSounds();
    const QString defaultNotificationSound = QStringLiteral(DEFAULT_NOTIFICATION_SOUND);
    return notificationSounds.contains(defaultNotificationSound)
        ? defaultNotificationSound
        : (notificationSounds.isEmpty()
            ? QString()
            : notificationSounds.first());
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveSteamWebApiKey(const QString &webApiKey)
{
    const bool encrypted = SaveEncryptedWebApiKey(webApiKey);
    m_tempEncryptionKey.clear();

    if (!encrypted)
    {
        return false;
    }

    m_settings.sync();

    const bool saved = (m_settings.status() == QSettings::NoError);
    if (saved)
    {
        m_steamWebApiKey = "api_key_set";
        emit signalConfigChanged();
    }

    return saved;
}

/////////////////////////////////////////////////////////////////////

void Settings::SavePlainValues()
{
    m_settings.beginGroup(GROUP_APPEARANCE);
    m_settings.setValue("Theme", m_theme);
    m_settings.setValue("ShowLymalinkLogo", m_showLymalinkLogo);
    m_settings.setValue("Language", m_language);
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_INTERFACE);
    m_settings.setValue("CloseToTray", m_closeToTray);
    m_settings.setValue("CloseToTrayToast", m_closeToTrayToast);
    m_settings.setValue("ShowCollapseButton", m_showCollapseButton);
    m_settings.setValue("ShowTooltips", m_showTooltips);
    m_settings.setValue("EnableCollapseBorderButton", m_enableCollapseBorderButton);
    m_settings.setValue("SidebarCollapsed", m_sidebarCollapsed);
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_DISPLAY);
    m_settings.setValue("GlobalColorStyle", m_globalColorStyle);
    m_settings.setValue("ProgressFrameColorStyle", m_progressFrameColorStyle);
    m_settings.setValue("ProgressBarColorStyle", m_progressBarColorStyle);
    m_settings.setValue("ShowProgressFrame", m_showProgressFrame);
    m_settings.setValue("ShowProgressBar", m_showProgressBar);
    m_settings.setValue("ShowInstallationStatusBadge", m_showInstallationStatusBadge);
    m_settings.setValue("ProgressFrameGrayscaleMode", m_progressFrameGrayscaleMode);
    m_settings.setValue("ShowTotalAchievementsBadge", m_showTotalAchievementsBadge);
    m_settings.setValue("ShowTargetTypeBadge", m_showTargetTypeBadge);
    m_settings.setValue("EnableProgressFrameCompletionAnimation", m_enableProgressFrameCompletionAnimation);
    m_settings.setValue("EnableDynamicAchievementRows", m_enableDynamicAchievementRows);
    m_settings.setValue("WindowSizeX", m_windowSizeX);
    m_settings.setValue("WindowSizeY", m_windowSizeY);
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    m_settings.setValue("SteamId", m_steamId);
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_BACKGROUND_SERVICE);
    m_settings.setValue("NotificationSound", m_notificationSound);
    m_settings.setValue("OverlayNotificationPosition", m_overlayNotificationPosition);
    m_settings.setValue("CustomNotificationSound", m_customNotificationSound);
    m_settings.setValue("CustomNotificationSoundPath", m_customNotificationSoundPath);
    m_settings.endGroup();

    m_settings.beginGroup(GROUP_DASHBOARD);
    m_settings.setValue("ToolbarSort", m_dashboardToolbarSort);
    m_settings.setValue("ToolbarFilters", m_dashboardToolbarFilters);
    m_settings.setValue("ToolbarSortDescending", m_dashboardToolbarSortDescending);
    m_settings.setValue("ToolbarLayout", m_dashboardToolbarLayout);
    m_settings.setValue("WelcomeHelpText", m_welcomeHelpText);
    m_settings.setValue("TargetDetailsHelpText", m_targetDetailsHelpText);
    m_settings.endGroup();
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveEncryptedWebApiKey(const QString &webApiKey)
{
    if (webApiKey.isEmpty())
    {
        qDebug() << "Settings::SaveEncryptedWebApiKey() - Nothing to save";
        return true;
    }

    Encryption encryption;
    if (m_tempEncryptionKey.isEmpty())
    {
        qDebug() << "Settings::SaveEncryptedWebApiKey() - Missing temporary encryption key";
        return false;
    }

    const QString encryptedWebApiKey = encryption.Encrypt(webApiKey, m_tempEncryptionKey);
    if (encryptedWebApiKey.isEmpty())
    {
        qDebug() << "Settings::SaveEncryptedWebApiKey() - Creating encryptedWebApiKey failed";
        return false;
    }

    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    m_settings.setValue("WebApiKey", encryptedWebApiKey);
    m_settings.endGroup();

    return true;
}

/////////////////////////////////////////////////////////////////////

void Settings::LoadEncryptedValueState()
{
    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    const QString encryptedWebApiKey = m_settings.value("WebApiKey").toString();
    m_settings.endGroup();

    if (encryptedWebApiKey.isEmpty())
    {
        m_steamWebApiKey.clear();
        return;
    }

    m_steamWebApiKey = "api_key_set";
}
