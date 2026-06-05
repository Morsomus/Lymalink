/////////////////////////////////////////////////////////
// File: Main.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Entry point for the Lymalink application
//              frontend.
/////////////////////////////////////////////////////////

import app.settings 1.0 as AppSettings

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: id_root

    readonly property int defaultMinimumWidth: 900

    visible: true
    width: ctxSettings.windowSizeX
    height: ctxSettings.windowSizeY
    title: qsTr("Lymalink")
    minimumWidth: id_sidebar.p_currentPage === 0 ? id_dashboard.requiredWindowMinimumWidth : defaultMinimumWidth
    minimumHeight: 700

    onMinimumWidthChanged: {
        if (width < minimumWidth) {
            width = minimumWidth
        }
    }

    Component.onCompleted: {
        if (ctxSettings.welcomeHelpText !== LYMALINK_APP_VERSION) {
            id_welcomeHelpTextMarkdownPopup.openDocument(qsTr("Welcome"), USER_GUIDE_0_8_0_BETA_MD_TEXT)
        }
    }

    background: Rectangle {
        color: "#181818"
    }

    onClosing: function(close) {
        if (ctxSettings.closeToTray && ctxSysTray.available) {
            close.accepted = false
            id_root.hide()          // Hide Window
            
            ctxSysTray.SetTrayIconVisibility(true)

            if (ctxSettings.closeToTrayToast) {
                ctxSysTray.ShowToastNotification(
                    "Running in background",
                    "Application minimized to tray"
                )
            }
        }
    }

    Connections {
        target: ctxSysTray

        function onSignalOpenWindow() {
            if (ctxSettings.closeToTray) {
                ctxSysTray.SetTrayIconVisibility(false)
                id_root.show()
                id_root.raise()
                id_root.requestActivate()
            }
        }
    }

    Connections {
        target: ctxLymalink

        function onSignalErrorOccurred(title, message) {
            id_errorPopup.showError(title, message)
        }
    }

    ErrorPopup {
        id: id_errorPopup
    }

    MarkdownDocumentPopup {
        id: id_welcomeHelpTextMarkdownPopup
        onClosed: {
            if (ctxSettings.welcomeHelpText !== LYMALINK_APP_VERSION) {
                ctxSettings.SaveValue(AppSettings.Settings.WelcomeHelpText, LYMALINK_APP_VERSION)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: id_sidebar
        }

        // Main content area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1f1f1f"

            // Subtle left divider from sidebar
            Rectangle {
                width: 1
                height: parent.height
                anchors.left: parent.left
                color: "#2a2a2a"
            }

            StackLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                currentIndex: id_sidebar.p_currentPage

                Dashboard {
                    id: id_dashboard
                }
                Settings {}
            }
        }
    }
}
