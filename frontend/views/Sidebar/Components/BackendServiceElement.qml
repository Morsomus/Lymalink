/////////////////////////////////////////////////////////
// File: BackendServiceElement.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: BackendServiceElement QML Component for
//              displaying user backend service information
//              and setting.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: id_root

    property bool collapsed: false

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
        enabled: id_root.collapsed
        hoverEnabled: true

        CustomTooltip {
            active: id_rootMouseArea.containsMouse
            delay: 300
            text: qsTr("Keep tracking active even when the application is closed")
        }

        onClicked: {
            id_backgroundServiceSwitch.checked = !id_backgroundServiceSwitch.checked
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
                    color: id_backgroundServiceSwitch.checked ? Themes.general.colors.statusActive : Themes.general.colors.statusInactive
                    font.pixelSize: id_root.collapsed ? Themes.general.fontSizes.statusCollapsed : Themes.general.fontSizes.statusExpanded
                }

                Label {
                    Layout.fillWidth: true
                    visible: !id_root.collapsed
                    text: qsTr("Background service")
                    color: Themes.general.colors.titleText
                    font.pixelSize: Themes.general.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }

                Switch {
                    id: id_backgroundServiceSwitch
                    
                    Layout.preferredWidth: id_root.collapsed ? 36 : 46
                    checked: false
                    focusPolicy: Qt.NoFocus
                    visible: id_root.collapsed == false

                    CustomTooltip {
                        active: id_backgroundServiceSwitch.hovered
                        delay: 300
                        text: qsTr("Keep tracking active even when the application is closed")
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !id_root.collapsed
                text: id_backgroundServiceSwitch.checked ? qsTr("Background tracking on") : qsTr("Only tracks while application is started")
                color: Themes.general.colors.bodyText
                font.pixelSize: Themes.general.fontSizes.body
                elide: Text.ElideRight
            }
        }
    }
}
