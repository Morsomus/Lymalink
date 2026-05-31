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
import QtQuick.Effects

Item {
    id: id_root

    // Public ________________________________________________
    property int p_currentPage: 0
    property bool p_collapsed: false
    property real p_expandedWidth: 260
    property real p_collapsedWidth: 72

    // Internals _____________________________________________
    property real panelWidth: id_root.p_collapsed ? id_root.p_collapsedWidth : id_root.p_expandedWidth
    readonly property bool hideLogo: !ctxSettings.showLymalinkLogo
    readonly property bool disableCollapseBorder: !ctxSettings.enableCollapseBorderButton
    readonly property bool disableCollapseButton: !ctxSettings.showCollapseButton
    readonly property bool dbusServiceReady: typeof ctxDBusService !== "undefined" && ctxDBusService !== null
    readonly property var activeTargetIds: id_root.dbusServiceReady ? ctxDBusService.activeTargetIds : []
    readonly property int currentPlayingCount: id_root.activeTargetIds.length
    readonly property string currentPlayingTitle: id_root.currentPlayingCount > 0 ? targetTitle(id_root.activeTargetIds[0]) : ""
    readonly property string currentPlayingSummary: id_root.currentPlayingTitle + (id_root.currentPlayingCount > 1 ? " (+" + (id_root.currentPlayingCount - 1) + ")" : "")
    readonly property string currentPlayingTooltip: id_root.currentPlayingCount > 0
        ? qsTr("Currently playing:") + "\n" + targetTitles().join("\n")
        : qsTr("Currently playing:") + "\n" + qsTr("Nothing")

    onP_collapsedChanged: {
        if (id_root.p_collapsed !== ctxSettings.sidebarCollapsed) {
            ctxSettings.SaveValue(Settings.SidebarCollapsed, id_root.p_collapsed)
        }
    }

    Component.onCompleted: p_collapsed = ctxSettings.sidebarCollapsed

    Connections {
        target: ctxSettings

        function onSignalDefaultsReset() {
            id_root.p_collapsed = ctxSettings.sidebarCollapsed
        }
    }

    Layout.preferredWidth: id_root.panelWidth
    Layout.fillHeight: true

    clip: false

    Behavior on panelWidth {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    function targetTitle(targetId) {
        if (typeof ctxLymalink === "undefined" || ctxLymalink === null) {
            return "#" + targetId
        }

        const title = ctxLymalink.GetTargetTitle(Number(targetId))
        return title.length > 0 ? title : "#" + targetId
    }

    function targetTitles() {
        const titles = []
        for (let i = 0; i < activeTargetIds.length; ++i) {
            titles.push(targetTitle(activeTargetIds[i]))
        }
        return titles
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

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
            Item {
                id: id_logoSlot

                property real animatedHeight: id_root.hideLogo ? 37 : (id_root.p_collapsed ? 64 : 220)

                Layout.fillWidth: true
                Layout.preferredWidth: id_root.p_collapsed ? 64 : 220
                Layout.preferredHeight: animatedHeight
                clip: true

                Behavior on animatedHeight {
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.OutCubic
                    }
                }

                Image {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, id_root.p_collapsed ? 64 : 220)
                    height: Math.min(parent.height, id_root.p_collapsed ? 64 : 220)
                    opacity: id_root.hideLogo ? 0.0 : 1.0
                    visible: !id_root.hideLogo || opacity > 0.0
                    source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00002_E.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 140
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            Rectangle {
                visible: !id_root.hideLogo
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Themes.sidebar.colors.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                SidebarButton {
                    Layout.fillWidth: true
                    p_collapsed: id_root.p_collapsed
                    p_selected: id_root.p_currentPage === 0
                    p_iconText: "□"
                    p_iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00005_ED.png"
                    p_label: qsTr("Dashboard")
                    onClicked: id_root.p_currentPage = 0
                }

                SidebarButton {
                    Layout.fillWidth: true
                    p_collapsed: id_root.p_collapsed
                    p_selected: id_root.p_currentPage === 1
                    p_iconText: "⚙"
                    p_iconUrl: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                    p_label: qsTr("Settings")
                    onClicked: id_root.p_currentPage = 1
                }
            }

            Item {
                Layout.fillHeight: true
            }

            Rectangle {
                id: id_currentlyPlaying

                Layout.fillWidth: true
                Layout.preferredHeight: id_root.p_collapsed ? 42 : 56
                color: Themes.general.colors.background
                border.color: Themes.general.colors.border
                border.width: 1
                radius: 8

                MouseArea {
                    id: id_currentlyPlayingMouseArea

                    z: 2
                    anchors.fill: parent
                    hoverEnabled: true

                    CustomTooltip {
                        p_alwaysVisible: true
                        p_active: (id_currentlyPlayingMouseArea.containsMouse && id_root.currentPlayingCount > 1) || (id_currentlyPlayingMouseArea.containsMouse && id_root.p_collapsed)
                        p_delay: 300
                        p_text: id_root.currentPlayingTooltip
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: id_root.p_collapsed ? 0 : 10
                    spacing: id_root.p_collapsed ? 0 : 8

                    Item {
                        Layout.fillWidth: id_root.p_collapsed
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22

                        Image {
                            id: id_currentlyPlayingIcon

                            anchors.centerIn: parent
                            source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00037_ED.png"
                            width: 22
                            height: 22
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: false // MultiEffect draws image
                        }

                        MultiEffect {
                            anchors.fill: id_currentlyPlayingIcon
                            source: id_currentlyPlayingIcon
                            colorizationColor: id_root.currentPlayingCount > 0
                                ? Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)
                                : Themes.general.colors.bodyText
                            colorization: 1.0
                            opacity: id_root.currentPlayingCount > 0 ? 1.0 : 0.45
                        }
                    }

                    ColumnLayout {
                        visible: !id_root.p_collapsed
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Currently playing")
                            color: Themes.general.colors.bodyText
                            font.pixelSize: Themes.general.fontSizes.body
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: id_root.currentPlayingCount > 0 ? id_root.currentPlayingSummary : qsTr("Nothing")
                            color: id_root.currentPlayingCount > 0 ? Themes.general.colors.titleText : Themes.general.colors.bodyText
                            font.pixelSize: Themes.general.fontSizes.title
                            font.bold: id_root.currentPlayingCount > 0
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            BackendServiceElement {
                p_collapsed: id_root.p_collapsed
                onClicked: id_root.p_currentPage = 1
            }

            GHElement {
                p_collapsed: id_root.p_collapsed
                p_linkUrl: "https://github.com/Morsomus/Lymalink"
                Layout.fillWidth: true
                Layout.preferredHeight: 42
            }

            // Version Info
            Label {
                Layout.fillWidth: true
                text: id_root.p_collapsed ? "" : "Lymalink • v" + LYMALINK_APP_VERSION
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
                onClicked: id_root.p_collapsed = !id_root.p_collapsed
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
        text: id_root.p_collapsed ? ">" : "<"
        focusPolicy: Qt.NoFocus
        onClicked: id_root.p_collapsed = !id_root.p_collapsed

        CustomTooltip {
            p_active: id_collapseButton.hovered
            p_delay: 300
            p_text: id_root.p_collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
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
