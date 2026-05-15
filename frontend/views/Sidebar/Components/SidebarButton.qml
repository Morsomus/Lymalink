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

    Layout.preferredHeight: 42
    leftPadding: collapsed ? 0 : 10

    flat: true
    focusPolicy: Qt.NoFocus

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

        // Fallback for iconText if iconUrl is not provided
        Label {
            Layout.preferredWidth: 32
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: iconUrl ? false : true
            text: id_root.iconText
            color: id_root.selected ? Themes.sidebarButton.colors.navTextSelected : Themes.sidebarButton.colors.navText
            font.pixelSize: Themes.sidebarButton.fontSizes.navIcon
        }

        Label {
            Layout.fillWidth: true
            visible: !id_root.collapsed
            opacity: id_root.collapsed ? 0 : 1
            text: id_root.label
            color: id_root.selected ? Themes.sidebarButton.colors.navTextSelected : Themes.sidebarButton.colors.navText
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
        radius: 8
        color: id_root.down 
            ? Themes.sidebarButton.colors.navBackgroundPressed
            : (id_root.selected 
                ? Themes.sidebarButton.colors.navBackgroundSelected 
                : (id_root.hovered 
                    ? Themes.sidebarButton.colors.navBackgroundHover 
                    : Themes.sidebarButton.colors.navBackground))

        Behavior on color {
            enabled: !id_root.down
            
            ColorAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }
}