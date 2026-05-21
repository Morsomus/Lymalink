/////////////////////////////////////////////////////////
// File: BackendServiceElement.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: BackendServiceElement QML Component for
//              displaying user backend service information
//              and status.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: id_root

    property bool collapsed: false
    signal clicked()

    readonly property bool backendAvailable: typeof ctxLymalink !== "undefined" && ctxLymalink !== null
    readonly property int serviceState: !backendAvailable ? 0 : (ctxSettings.backendService ? 2 : 1)
    readonly property color serviceColor: {
        switch (serviceState) {
            case 2: return Themes.general.colors.statusActive
            case 1: return "#f2c94c"
            default: return "#d35f5f"
        }
    }
    readonly property string serviceStatusText: {
        switch (serviceState) {
            case 2: return qsTr("Tracking independently, app can be closed")
            case 1: return qsTr("Tracking while app is open only")
            default: return qsTr("Service error, tracking unavailable")
        }
    }
    readonly property string serviceTooltip: {
        switch (serviceState) {
            case 2: return qsTr("Background service is running independently. Achievement tracking and notifications work even when the application is closed.")
            case 1: return qsTr("Background service is disabled. Achievement tracking and notifications only work while the application is open.")
            default: return qsTr("Background service encountered an error and is not running. Achievement tracking is unavailable.")
        }
    }

    Layout.fillWidth: true
    Layout.preferredHeight: id_root.collapsed ? 52 : 72
    color: id_rootMouseArea.pressed ? Themes.general.colors.backgroundPressed : (id_rootMouseArea.containsMouse ? Themes.general.colors.backgroundHover : Themes.general.colors.background)
    border.color: Themes.general.colors.border
    radius: 8
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: 100
        }
    }
    
    MouseArea {
        id: id_rootMouseArea

        anchors.fill: parent
        hoverEnabled: true
        onClicked: id_root.clicked()

        CustomTooltip {
            active: id_rootMouseArea.containsMouse
            delay: 300
            text: id_root.serviceTooltip
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: id_root.collapsed ? 0 : 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: id_root.collapsed ? 0 : 8

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "●"
                    color: id_root.serviceColor
                    font.pixelSize: id_root.collapsed ? Themes.general.fontSizes.statusCollapsed : Themes.general.fontSizes.statusExpanded
                }

                Label {
                    Layout.fillWidth: true
                    visible: !id_root.collapsed
                    text: qsTr("Background service status")
                    color: Themes.general.colors.titleText
                    font.pixelSize: Themes.general.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !id_root.collapsed
                text: id_root.serviceStatusText
                color: Themes.general.colors.bodyText
                font.pixelSize: Themes.general.fontSizes.body
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }
}
