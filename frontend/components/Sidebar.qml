/////////////////////////////////////////////////////////
// File: Sidebar.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Sidebar QML Component for Lymalink
//              Application Frontend.
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import app.themes 1.0

Item {
    id: root

    property int currentPage: 0
    property bool collapsed: false
    property real expandedWidth: 260
    property real collapsedWidth: 72
    property real panelWidth: collapsed ? collapsedWidth : expandedWidth

    Layout.preferredWidth: panelWidth
    Layout.fillHeight: true

    clip: false

    Behavior on panelWidth {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        color: Themes.sidebar.colors.panel

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 14
            spacing: 10

            Image {
                Layout.fillWidth: true
                Layout.preferredWidth: collapsed ? 64 : 220
                Layout.preferredHeight: collapsed ? 64 : 220
                source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00002_E.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Themes.sidebar.colors.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                SidebarButton {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    selected: root.currentPage === 0
                    iconText: "□"
                    iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00005_ED.png"
                    label: qsTr("Dashboard")
                    onClicked: root.currentPage = 0
                }

                SidebarButton {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    selected: root.currentPage === 1
                    iconText: "⚙"
                    iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                    label: qsTr("Settings")
                    onClicked: root.currentPage = 1
                }
            }

            Item { Layout.fillHeight: true }

            BackendServiceElement {
                collapsed: root.collapsed 
            }

            GHElement {
                collapsed: root.collapsed 
                linkUrl: "https://github.com/Morsomus/Lymalink"
                Layout.fillWidth: true
                Layout.preferredHeight: 42
            }

            // Version Info
            Label {
                Layout.fillWidth: true
                text: root.collapsed ? "" : qsTr("v" + LYMALINK_APP_VERSION + "  •  GPLv3")
                color: Themes.sidebar.colors.versionText
                font.pixelSize: Themes.sidebar.fontSizes.version
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        Rectangle {
            width: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            color: Themes.sidebar.colors.divider
        }
    }

    Button {
        id: collapseButton
        width: 34
        height: 48
        anchors.right: parent.right
        anchors.rightMargin: -17
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -6
        z: 10
        text: root.collapsed ? ">" : "<"
        focusPolicy: Qt.NoFocus
        ToolTip.visible: hovered
        ToolTip.delay: 300
        ToolTip.text: root.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
        onClicked: root.collapsed = !root.collapsed

        contentItem: Label {
            anchors.left: parent.left
            anchors.leftMargin: -8
            text: collapseButton.text
            color: Themes.sidebar.colors.collapseText
            font.pixelSize: Themes.sidebar.fontSizes.collapseButton
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: collapseButton.hovered ? Themes.sidebar.colors.collapseBackgroundHover : Themes.sidebar.colors.collapseBackground
            border.color: Themes.sidebar.colors.collapseBorder
            border.width: 1
        }
    }

    component SidebarButton: Button {
        id: navButton

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
            }

            // Fallback for iconText if iconUrl is not provided
            Label {
                Layout.preferredWidth: 32
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                visible: iconUrl ? false : true
                text: navButton.iconText
                color: navButton.selected ? Themes.sidebar.colors.navTextSelected : Themes.sidebar.colors.navText
                font.pixelSize: Themes.sidebar.fontSizes.navIcon
            }

            Label {
                Layout.fillWidth: true
                visible: !navButton.collapsed
                opacity: navButton.collapsed ? 0 : 1
                text: navButton.label
                color: navButton.selected ? Themes.sidebar.colors.navTextSelected : Themes.sidebar.colors.navText
                font.pixelSize: Themes.sidebar.fontSizes.navLabel
                font.bold: navButton.selected
                elide: Text.ElideRight

                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }
            }
        }

        background: Rectangle {
            radius: 8
            color: navButton.selected ? Themes.sidebar.colors.navBackgroundSelected : (navButton.hovered ? Themes.sidebar.colors.navBackgroundHover : Themes.sidebar.colors.navBackground)
        }
    }
}
