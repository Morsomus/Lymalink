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

    // Public ________________________________________________
    property bool p_collapsed: false
    
    signal clicked()

    // Internals _____________________________________________
    readonly property bool dbusServiceReady: typeof ctxDBusService !== "undefined" && ctxDBusService !== null
    readonly property bool serviceAvailable: dbusServiceReady && ctxDBusService.serviceAvailable
    readonly property bool serviceActive: dbusServiceReady && ctxDBusService.serviceActive
    readonly property bool serviceEnabled: dbusServiceReady && ctxDBusService.serviceEnabled
    readonly property bool serviceStarting: dbusServiceReady && ctxDBusService.serviceStarting
    readonly property bool serviceHealthy: serviceAvailable && serviceActive
    readonly property int serviceState: serviceStarting ? 3 : (serviceHealthy ? (serviceEnabled ? 2 : 1) : 0)
    readonly property color serviceColor: {
        switch (serviceState) {
            case 3: return Themes.serviceIndicator.colors.starting
            case 2: return Themes.serviceIndicator.colors.running
            case 1: return Themes.serviceIndicator.colors.running
            default: return Themes.serviceIndicator.colors.error
        }
    }
    readonly property string serviceStatusText: {
        switch (serviceState) {
            case 3: return qsTr("Starting background service...")
            case 2: return qsTr("Tracking in the background\nLymalink can be closed")
            case 1: return qsTr("Tracking only while Lymalink is open")
            default: return qsTr("Error: Background service unavailable")
        }
    }
    readonly property string serviceTooltip: {
        switch (serviceState) {
            case 3: return qsTr("Background service is starting. Tracking will resume when the service responds.")
            case 2: return qsTr("Background service is running independently. Achievement tracking and notifications work even when Lymalink is closed.")
            case 1: return qsTr("Background service is active and responding. Achievement tracking and notifications work only when Lymalink is open.")
            default: return qsTr("Background service did not respond. Achievement tracking is unavailable.")
        }
    }

    Layout.fillWidth: true
    Layout.preferredHeight: id_root.p_collapsed ? 52 : 72
    color: id_rootMouseArea.pressed ? Themes.general.colors.backgroundPressed : (id_rootMouseArea.containsMouse ? Themes.general.colors.backgroundHover : Themes.general.colors.background)
    border.color: Themes.general.colors.border
    radius: 8
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: 100
        }
    }
    
    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    MouseArea {
        id: id_rootMouseArea

        anchors.fill: parent
        hoverEnabled: true
        onClicked: id_root.clicked()

        CustomTooltip {
            p_active: id_rootMouseArea.containsMouse
            p_delay: 300
            p_text: id_root.serviceTooltip
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: id_root.p_collapsed ? 0 : 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: id_root.p_collapsed ? 0 : 8

                Label {
                    id: id_serviceDot

                    readonly property bool breathing: id_root.serviceState === 1

                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "●"
                    color: id_root.serviceColor
                    font.pixelSize: id_root.p_collapsed ? Themes.general.fontSizes.statusCollapsed : Themes.general.fontSizes.statusExpanded
                    onBreathingChanged: if (!breathing) opacity = Themes.serviceIndicator.opacity.solid

                    SequentialAnimation on opacity {
                        running: id_serviceDot.breathing
                        loops: Animation.Infinite

                        NumberAnimation {
                            to: Themes.serviceIndicator.opacity.breathingLow
                            duration: Themes.serviceIndicator.animation.breathingDuration
                            easing.type: Easing.InOutSine
                        }
                        NumberAnimation {
                            to: Themes.serviceIndicator.opacity.solid
                            duration: Themes.serviceIndicator.animation.breathingDuration
                            easing.type: Easing.InOutSine
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: !id_root.p_collapsed
                    text: qsTr("Background service status")
                    color: Themes.general.colors.titleText
                    font.pixelSize: Themes.general.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !id_root.p_collapsed
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
