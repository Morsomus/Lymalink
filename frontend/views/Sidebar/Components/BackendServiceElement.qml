/////////////////////////////////////////////////////////
// File: BackendServiceElement.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: BackendServiceElement QML Component for
//              displaying user backend service information
//              and setting.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool collapsed: false

    Layout.fillWidth: true
    Layout.preferredHeight: root.collapsed ? 52 : 72

    color: rootBg.pressed ? Themes.general.colors.backgroundPressed : (rootBg.containsMouse ? Themes.general.colors.backgroundHover : Themes.general.colors.background)
    border.color: Themes.general.colors.border

    radius: 8
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: 100
        }
    }
    
    MouseArea {
        id: rootBg

        anchors.fill: parent
        enabled: root.collapsed
        hoverEnabled: true

        // Mouse area doesn't have ToolTip property by default, so we need to create one manually.
        ToolTip {
            text: qsTr("Keep tracking active even when the app is closed")
            visible: rootBg.containsMouse
            delay: 300
        }

        onClicked: {
            serviceSwitch.checked = !serviceSwitch.checked
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.collapsed ? 0 : 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: root.collapsed ? 0 : 8

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "●"
                    color: serviceSwitch.checked ? Themes.general.colors.statusActive : Themes.general.colors.statusInactive
                    font.pixelSize: root.collapsed ? Themes.general.fontSizes.statusCollapsed : Themes.general.fontSizes.statusExpanded
                }

                Label {
                    Layout.fillWidth: true
                    visible: !root.collapsed
                    text: qsTr("Background service")
                    color: Themes.general.colors.titleText
                    font.pixelSize: Themes.general.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }

                Switch {
                    id: serviceSwitch
                    
                    Layout.preferredWidth: root.collapsed ? 36 : 46
                    checked: false
                    focusPolicy: Qt.NoFocus
                    visible: root.collapsed == false
                    ToolTip.visible: hovered
                    ToolTip.delay: 300
                    ToolTip.text: qsTr("Keep tracking active even when the app is closed")
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !root.collapsed
                text: serviceSwitch.checked ? qsTr("Background tracking on") : qsTr("Only tracks while open")
                color: Themes.general.colors.bodyText
                font.pixelSize: Themes.general.fontSizes.body
                elide: Text.ElideRight
            }
        }
    }
}
