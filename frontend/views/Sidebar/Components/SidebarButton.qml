/////////////////////////////////////////////////////////
// File: SidebarButton.qml
// Date: 2026-05-06
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Sidebar button
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: id_root

    property bool collapsed: false
    property bool selected: false
    property string iconText: ""
    property string iconUrl: ""
    property string label: ""
    readonly property color themedProgressColor: Themes.globalStyle.progressColor(ctxSettings.globalColorStyle)
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)

    Layout.preferredHeight: 42
    leftPadding: collapsed ? 0 : 10
    flat: true
    focusPolicy: Qt.NoFocus

    // Pulse animation state
    property real pulseOpacity: 0.0
    property real pulseScale: 0.85

    onClicked: pulseAnim.restart()

    SequentialAnimation {
        id: pulseAnim

        ParallelAnimation {
            NumberAnimation {
                target: id_root
                property: "pulseOpacity"
                from: 0.45
                to: 0.0
                duration: 400
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: id_root
                property: "pulseScale"
                from: 0.85
                to: 1.0
                duration: 400
                easing.type: Easing.OutCubic
            }
        }
    }

    CustomTooltip {
        active: id_root.collapsed && id_root.hovered
        delay: 300
        text: id_root.label
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.fillWidth: collapsed ? true : false
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            visible: iconUrl ? true : false
            source: iconUrl
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Label {
            Layout.preferredWidth: 32
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: iconUrl ? false : true
            text: id_root.iconText
            color: id_root.selected
                ? id_root.themedCompletionColor
                : Themes.sidebarButton.colors.navText
            font.pixelSize: Themes.sidebarButton.fontSizes.navIcon
        }

        Label {
            Layout.fillWidth: true
            visible: !id_root.collapsed
            opacity: id_root.collapsed ? 0 : 1
            text: id_root.label
            color: id_root.selected
                ? id_root.themedCompletionColor
                : Themes.sidebarButton.colors.navText
            font.pixelSize: Themes.sidebarButton.fontSizes.navLabel
            font.bold: id_root.selected
            elide: Text.ElideRight

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }
    }

    background: Rectangle {
        id: id_bg

        radius: 8

        // Base fill color
        color: id_root.down
            ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.20)
            : (id_root.selected
                ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.16)
                : (id_root.hovered
                    ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.08)
                    : Themes.sidebarButton.colors.navBackground))

        Behavior on color {
            enabled: !id_root.down
            ColorAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }

        // Selected-state
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: id_root.themedCompletionColor
            opacity: id_root.selected ? 0.55 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }
        }

        // Inner top-highlight streak
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            height: parent.height * 0.45
            radius: parent.radius
            opacity: id_root.selected ? 0.12 : (id_root.hovered ? 0.06 : 0.0)
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#ffffff"
                }
                GradientStop {
                    position: 1.0
                    color: "transparent"
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }
        }

        // Pulse ripple overlay
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: id_root.themedCompletionColor
            opacity: id_root.pulseOpacity
            scale: id_root.pulseScale
            transformOrigin: Item.Center
        }
    }
}
