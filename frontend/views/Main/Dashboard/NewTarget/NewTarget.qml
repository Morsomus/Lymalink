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
    property string activeTarget: ""
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
            description: qsTr("Import official Steam achievements with Steam Web API."),
            enabled: false
        },
        {
            key: "custom",
            label: qsTr("Custom"),
            icon: "✦",
            description: qsTr("Track logs, saves, or text files with your own rules."),
            enabled: false
        }
    ]

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    Loader {
        anchors.fill: parent
        active: id_root.activeTarget === "emulator"
        visible: active
        sourceComponent: Component {
            Emulator {
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 77, 576)
        spacing: 12
        visible: id_root.activeTarget === ""

        Repeater {
            model: id_root.targetTypeModel

            delegate: Rectangle {
                id: id_targetType

                readonly property bool isEnabled: modelData.enabled
                readonly property bool isHovered: id_targetTypeMouseArea.containsMouse
                readonly property bool isPressed: id_targetTypeMouseArea.pressed
                readonly property color defaultBackground: isPressed
                    ? Themes.newTarget.colors.cardBackgroundPressed
                    : isHovered
                        ? Themes.newTarget.colors.cardBackgroundHover
                        : Themes.newTarget.colors.cardBackground
                readonly property color defaultBorder: isHovered
                    ? Themes.newTarget.colors.cardBorderHover
                    : Themes.newTarget.colors.cardBorder

                Layout.fillWidth: true
                height: 96
                radius: 10
                opacity: isEnabled ? 1.0 : 0.72
                color: defaultBackground
                border.width: 1
                border.color: defaultBorder

                states: State {
                    name: "disabled"
                    when: !id_targetType.isEnabled
                    PropertyChanges {
                        id_targetType.color: Themes.newTarget.colors.cardBackground
                    }
                    PropertyChanges {
                        id_targetType.border.color: Themes.newTarget.colors.cardBorder
                    }
                }

                Behavior on color {
                    ColorAnimation {
                        duration: 100
                    }
                }

                Behavior on border.color {
                    ColorAnimation {
                        duration: 100
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 19

                    Text {
                        text: modelData.icon
                        font.pixelSize: Themes.newTarget.fontSizes.icon
                        color: Themes.newTarget.colors.icon
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

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
                            color: id_targetType.isEnabled
                                ? Themes.newTarget.colors.descriptionText
                                : Themes.newTarget.colors.disabledText
                            wrapMode: Text.WordWrap
                            lineHeight: 1.3
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: id_badgeText.implicitWidth + 20
                        radius: 6
                        visible: !id_targetType.isEnabled
                        color: Themes.newTarget.colors.badgeBackground
                        border.width: 1
                        border.color: Themes.newTarget.colors.badgeBorder

                        Text {
                            id: id_badgeText

                            anchors.centerIn: parent
                            text: qsTr("Soon")
                            font.pixelSize: Themes.newTarget.fontSizes.badge
                            font.weight: Font.Medium
                            color: Themes.newTarget.colors.badgeText
                        }
                    }

                    Text {
                        text: "›"
                        font.pixelSize: Themes.newTarget.fontSizes.arrow
                        color: id_targetType.isHovered
                            ? Themes.newTarget.colors.arrowHover
                            : Themes.newTarget.colors.arrow
                        opacity: id_targetType.isEnabled && id_targetType.isHovered ? 1.0 : 0.4

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 100
                            }
                        }
                    }
                }

                MouseArea {
                    id: id_targetTypeMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: id_targetType.isEnabled
                    cursorShape: id_targetType.isEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: id_root.activeTarget = modelData.key
                }
            }
        }
    }
}
