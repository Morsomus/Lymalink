/////////////////////////////////////////////////////////
// File: SidebarButton.qml
// Date: 2026-05-06
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Sidebar button
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property bool collapsed: false
    property bool selected: false
    property string iconText: ""
    property string iconUrl: ""
    property string label: ""

    Layout.preferredHeight: 42
    leftPadding: collapsed ? 0 : 10

    flat: true
    focusPolicy: Qt.NoFocus
    ToolTip.visible: collapsed && hovered
    ToolTip.delay: 300
    ToolTip.text: label

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

        // Fallback for iconText if iconUrl is not provided
        Label {
            Layout.preferredWidth: 32
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: iconUrl ? false : true
            text: root.iconText
            color: root.selected ? Themes.sidebarButton.colors.navTextSelected : Themes.sidebarButton.colors.navText
            font.pixelSize: Themes.sidebarButton.fontSizes.navIcon
        }

        Label {
            Layout.fillWidth: true
            visible: !root.collapsed
            opacity: root.collapsed ? 0 : 1
            text: root.label
            color: root.selected ? Themes.sidebarButton.colors.navTextSelected : Themes.sidebarButton.colors.navText
            font.pixelSize: Themes.sidebarButton.fontSizes.navLabel
            font.bold: root.selected
            elide: Text.ElideRight

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }
    }

    background: Rectangle {
        radius: 8
        color: root.down 
            ? Themes.sidebarButton.colors.navBackgroundPressed
            : (root.selected 
                ? Themes.sidebarButton.colors.navBackgroundSelected 
                : (root.hovered 
                    ? Themes.sidebarButton.colors.navBackgroundHover 
                    : Themes.sidebarButton.colors.navBackground))

        Behavior on color {
            enabled: !root.down
            
            ColorAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }
}