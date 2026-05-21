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
#include <QFileInfo>
#include <QTimer>
#include <QWindow>

/////////////////////////////////////////////////////////////////////

Settings::Settings(QObject *parent) : QObject(parent),
    m_settings(QSettings::IniFormat, QSettings::UserScope, ORGANIZATION, APPLICATION)
{
    m_windowSizeXDefault = 1510;
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

void Settings::SetPinCode(const QString &pinCode)
{
    m_pinCode = pinCode.isEmpty() ? DEFAULT_PIN_CODE : pinCode;

    // Refresh encrypted config values
    LoadEncryptedValues();
    emit signalConfigChanged();
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
    if (!SaveEncryptedValues())
    {
        return false;
    }

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
    m_showProgressFrame = m_settings.value("ShowProgressFrame", m_showProgressFrame).toBool();
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

    m_settings.beginGroup(GROUP_SYSTEM);
    m_backendService = m_settings.value("BackendService", m_backendService).toBool();
    m_settings.endGroup();

    LoadEncryptedValues();

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
        case ShowProgressFrame:
        {
            m_showProgressFrame = value.toBool();
            group = GROUP_DISPLAY;
            settingsKey = "ShowProgressFrame";
            settingsValue = m_showProgressFrame;
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
            QString val = value.toString();
            bool resetWebApiKey = val == "reset";
            m_steamWebApiKey = resetWebApiKey ? "" : val;
            if (resetWebApiKey)
            {
                group = GROUP_STEAM_WEB_API;
                settingsKey = "WebApiKey";
                settingsValue = m_steamWebApiKey;
                break;
            }
            else
            {
                return SaveSteamWebApiKey();
            }
        }
        case BackendService:
        {
            m_backendService = value.toBool();
            group = GROUP_SYSTEM;
            settingsKey = "BackendService";
            settingsValue = m_backendService;
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
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void Settings::SetDefaults()
{
    m_pinCode = DEFAULT_PIN_CODE;
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
    m_showProgressFrame = true;
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
    m_backendService = false;
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveSteamWebApiKey()
{
    if (!SaveEncryptedValues())
    {
        return false;
    }

    m_settings.sync();

    const bool saved = (m_settings.status() == QSettings::NoError);
    if (saved)
    {
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
    m_settings.setValue("ShowProgressFrame", m_showProgressFrame);
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

    m_settings.beginGroup(GROUP_SYSTEM);
    m_settings.setValue("BackendService", m_backendService);
    m_settings.endGroup();
}

/////////////////////////////////////////////////////////////////////

bool Settings::SaveEncryptedValues()
{
    if (m_steamWebApiKey.isEmpty())
    {
        qDebug() << "Settings::SaveEncryptedValues() - Nothing to save";
        return true;
    }

    Encryption encryption;
    const QString encryptionKey = m_pinCode.isEmpty() ? DEFAULT_PIN_CODE : m_pinCode;
    const QString encryptedWebApiKey = encryption.Encrypt(m_steamWebApiKey, encryptionKey);
    if (encryptedWebApiKey.isEmpty())
    {
        qDebug() << "Settings::SaveEncryptedValues() - Creating encryptedWebApiKey failed";
        return false;
    }

    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    m_settings.setValue("WebApiKey", encryptedWebApiKey);
    m_settings.endGroup();

    return true;
}

/////////////////////////////////////////////////////////////////////

void Settings::LoadEncryptedValues()
{
    qDebug() << "Settings::LoadEncryptedValues - Loading encrypted values from config...";

    m_settings.beginGroup(GROUP_STEAM_WEB_API);
    const QString encryptedWebApiKey = m_settings.value("WebApiKey").toString();
    m_settings.endGroup();

    if (encryptedWebApiKey.isEmpty())
    {
        qDebug() << "Settings::LoadEncryptedValues - No Encrypted Web API Key found";
        m_steamWebApiKey.clear();
        return;
    }

    Encryption encryption;
    const QString encryptionKey = m_pinCode.isEmpty() ? DEFAULT_PIN_CODE : m_pinCode;
    m_steamWebApiKey = encryption.Decrypt(encryptedWebApiKey, encryptionKey);
}
