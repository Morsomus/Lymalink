/////////////////////////////////////////////////////////
// File: NewTarget.qml
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Add new target view
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Internals _____________________________________________
    readonly property var targetTypeModel: [
        {
            key: "emulator",
            label: qsTr("Emulator"),
            icon: "🎮",
            description: qsTr("Track achievements from a local emulator such as Goldberg and CODEX."),
            enabled: true
        },
        {
            key: "steam",
            label: qsTr("Steam Import"),
            icon: "☁",
            description: qsTr("Import your official Steam achievements via the Steam Web API. Requires a personal API key."),
            enabled: false
        },
        {
            key: "custom",
            label: qsTr("Custom"),
            icon: "✦",
            description: qsTr("Monitor any file and define your own trigger rules. Works with logs, saves, or any text-based source."),
            enabled: false
        }
    ]

    /////////////////////////////////////////////////////////////////////

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 480)
        spacing: 10

        Repeater {
            model: id_root.targetTypeModel

            delegate: Rectangle {
                id: id_card
                Layout.fillWidth: true
                height: 80
                radius: 8
                opacity: modelData.enabled ? 1.0 : 0.5
                color: id_mouse.pressed
                    ? Themes.newTarget.colors.cardBackgroundPressed
                    : id_mouse.containsMouse
                        ? Themes.newTarget.colors.cardBackgroundHover
                        : Themes.newTarget.colors.cardBackground

                border.color: id_mouse.containsMouse
                    ? Themes.newTarget.colors.cardBorderHover
                    : Themes.newTarget.colors.cardBorder
                border.width: 1

                states: State {
                    name: "disabled"
                    when: !modelData.enabled
                    PropertyChanges { id_card.color: Themes.newTarget.colors.cardBackground }
                    PropertyChanges { id_card.border.color: Themes.newTarget.colors.cardBorder }
                }

                Behavior on color { ColorAnimation { duration: 100 } }
                Behavior on border.color { ColorAnimation { duration: 100 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 16

                    Text {
                        text: modelData.icon
                        font.pixelSize: Themes.newTarget.fontSizes.icon
                        color: Themes.newTarget.colors.icon
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: modelData.label
                            font.pixelSize: Themes.newTarget.fontSizes.label
                            font.weight: Font.Medium
                            color: Themes.newTarget.colors.labelText
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.description
                            font.pixelSize: Themes.newTarget.fontSizes.description
                            color: Themes.newTarget.colors.descriptionText
                            wrapMode: Text.WordWrap
                            lineHeight: 1.3
                        }
                    }

                    Text {
                        text: "›"
                        font.pixelSize: Themes.newTarget.fontSizes.arrow
                        color: id_mouse.containsMouse
                            ? Themes.newTarget.colors.arrowHover
                            : Themes.newTarget.colors.arrow
                        opacity: modelData.enabled && id_mouse.containsMouse ? 1.0 : 0.4
                        Behavior on opacity { NumberAnimation { duration: 100 } }
                    }
                }

                MouseArea {
                    id: id_mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: modelData.enabled
                    cursorShape: modelData.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    //onClicked: // Open correct frontend/views/Main/Dashboard/NewTarget/Targets/* -target view for setting up the target.
                }
            }
        }
    }
}
