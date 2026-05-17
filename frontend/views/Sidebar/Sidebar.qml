/////////////////////////////////////////////////////////
// File: Sidebar.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Sidebar QML Component for Lymalink
//              Application Frontend.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    property int currentPage: 0
    property bool collapsed: ctxSettings.sidebarCollapsed
    property real expandedWidth: 260
    property real collapsedWidth: 72
    property real panelWidth: collapsed ? collapsedWidth : expandedWidth
    property bool hideLogo: !ctxSettings.showLymalinkLogo
    property bool disableCollapseBorder: !ctxSettings.enableCollapseBorderButton
    property bool disableCollapseButton: !ctxSettings.showCollapseButton

    onCollapsedChanged: {
        ctxSettings.SaveValue(Settings.SidebarCollapsed, collapsed)
    }

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
        id: id_panel

        anchors.fill: parent
        color: Themes.sidebar.colors.panel

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 14
            spacing: 10

            // Logo
            Image {
                visible: !hideLogo
                Layout.fillWidth: true
                Layout.preferredWidth: collapsed ? 64 : 220
                Layout.preferredHeight: collapsed ? 64 : 220
                source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00002_E.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Item {
                visible: hideLogo
                Layout.preferredHeight: 37
            }

            Rectangle {
                visible: !hideLogo
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Themes.sidebar.colors.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                SidebarButton {
                    Layout.fillWidth: true
                    collapsed: id_root.collapsed
                    selected: id_root.currentPage === 0
                    iconText: "□"
                    iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00005_ED.png"
                    label: qsTr("Dashboard")
                    onClicked: id_root.currentPage = 0
                }

                SidebarButton {
                    Layout.fillWidth: true
                    collapsed: id_root.collapsed
                    selected: id_root.currentPage === 1
                    iconText: "⚙"
                    iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                    label: qsTr("Settings")
                    onClicked: id_root.currentPage = 1
                }
            }

            Item {
                Layout.fillHeight: true
            }

            BackendServiceElement {
                collapsed: id_root.collapsed 
            }

            GHElement {
                collapsed: id_root.collapsed 
                linkUrl: "https://github.com/Morsomus/Lymalink"
                Layout.fillWidth: true
                Layout.preferredHeight: 42
            }

            // Version Info
            Label {
                Layout.fillWidth: true
                text: id_root.collapsed ? "" : "v" + LYMALINK_APP_VERSION + "  •  " + LICENSE_APP_VERSION
                color: Themes.sidebar.colors.versionText
                font.pixelSize: Themes.sidebar.fontSizes.version
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        // Collapse border - Collapse sidebar
        Item {
            id: id_collapseBorder

            visible: !id_root.disableCollapseBorder
            width: 10
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            property bool showDivider: false

            MouseArea {
                id: id_collapseBorderMouseArea

                anchors.fill: id_collapseBorder
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: id_root.collapsed = !id_root.collapsed
	            onExited: id_collapseBorder.showDivider = false
            }

            // Timer to prevent collapse border flickering by passing mouse cursor
            Timer {
                interval: id_collapseBorderMouseArea.containsMouse ? 100 : 0
                running: id_collapseBorderMouseArea.containsMouse && !id_collapseBorder.showDivider
                repeat: false
                onTriggered: id_collapseBorder.showDivider = true
            }
            
            Rectangle {
                width: id_collapseBorder.width
                anchors.top: id_collapseBorder.top
                anchors.bottom: id_collapseBorder.bottom
                anchors.right: id_collapseBorder.right
                color: Themes.sidebar.colors.divider
                opacity: id_collapseBorder.showDivider ? 0.5 : 0.0

                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                    }
                }
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
        id: id_collapseButton
        
        visible: !id_root.disableCollapseButton
        width: 34
        height: 48
        anchors.right: parent.right
        anchors.rightMargin: -17
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -6
        z: 10
        text: id_root.collapsed ? ">" : "<"
        focusPolicy: Qt.NoFocus
        onClicked: id_root.collapsed = !id_root.collapsed

        CustomTooltip {
            active: id_collapseButton.hovered
            delay: 300
            text: id_root.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
        }

        contentItem: Label {
            anchors.left: parent.left
            anchors.leftMargin: -8
            text: id_collapseButton.text
            color: Themes.sidebar.colors.collapseText
            font.pixelSize: Themes.sidebar.fontSizes.collapseButton
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: id_collapseButton.down 
                ? Themes.sidebar.colors.collapseBackgroundPressed
                : (id_collapseButton.hovered 
                    ? Themes.sidebar.colors.collapseBackgroundHover 
                    : Themes.sidebar.colors.collapseBackground)

            border.color: Themes.sidebar.colors.collapseBorder
            border.width: 1
        }
    }
}
