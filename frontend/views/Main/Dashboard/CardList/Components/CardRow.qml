/////////////////////////////////////////////////////////
// File: CardRow.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: CardRow QML Component for displaying 
//              a tracked achievement or target in row format.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: id_root

    // Public ________________________________________________
    property string p_title: "Title"
    property string p_coverSource: ""     // Full cover image (fallback)
    property string p_logoSource: ""      // Prefer transparent library logo
    property int p_achievementCount: 0    // e.g. 5
    property int p_achievementTotal: 0    // e.g. 73
    property string p_status: ""          // "Installed" | "Not Installed"
    property string p_lastPlayed: ""      // e.g. "2 days ago"
    property string p_recentUnlock: ""    // e.g. "1 hour ago"
    property int p_delegateIndex: 0

    signal openTargetDetails(string title, string coverSource, int achievementCount, int achievementTotal, string status, string lastPlayed, string recentUnlock)

    // Internals _____________________________________________
    readonly property real progress: p_achievementTotal > 0 ? p_achievementCount / p_achievementTotal : 0.0
    readonly property bool isCompleted: p_achievementTotal > 0 && p_achievementCount >= p_achievementTotal
    readonly property bool hasVerticalScroll: ListView.view ? ListView.view.contentHeight > ListView.view.height : false
    readonly property int progressDelay: Math.max(0, Math.min(p_delegateIndex * 40, 300))
    property real animatedProgress: 0.0

    height: 64
    // width: set by parent (ListView delegate width: listView.width)
    clip: true
    color: Themes.cardRow.colors.rowBackground

    SequentialAnimation {
        id: id_progressIntroAnimation

        PauseAnimation {
            duration: id_root.progressDelay
        }
        NumberAnimation {
            target: id_root
            property: "animatedProgress"
            from: 0.0
            to: id_root.progress
            duration: 550
            easing.type: Easing.OutCubic
        }
    }

    Connections {
        target: id_root.ListView.view
        function onVisibleChanged() {
            if (id_root.ListView.view.visible) {
                id_root.restartProgressAnimation()
            }
        }
    }

    onProgressChanged: {
        if (!id_progressIntroAnimation.running) {
            restartProgressAnimation()
        }
    }

    Component.onCompleted: {
        Qt.callLater(restartProgressAnimation)
    }

    function restartProgressAnimation() {
        id_progressIntroAnimation.stop()
        animatedProgress = 0.0
        id_progressIntroAnimation.restart()
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Mouse Area for hover
    MouseArea {
        id: id_rootMouseArea

        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            id_root.openTargetDetails(
                id_root.p_title,
                id_root.p_coverSource,
                id_root.p_achievementCount,
                id_root.p_achievementTotal,
                id_root.p_status,
                id_root.p_lastPlayed,
                id_root.p_recentUnlock
            )
        }

        // Card Row Hover
        Rectangle {
            anchors {
                fill: parent
                rightMargin: id_root.hasVerticalScroll ? 30 : 0
            }
            color: id_rootMouseArea.pressed
                ? Themes.cardRow.colors.rowBackgroundPressed
                : (id_rootMouseArea.containsMouse
                    ? Themes.cardRow.colors.rowBackgroundHover
                    : Themes.cardRow.colors.rowBackground)
            // Hover border highlight
            border.width: id_rootMouseArea.containsMouse || id_rootMouseArea.pressed ? 1 : 0
            border.color: Themes.cardRow.colors.rowBorder
            radius: 6

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }
    
    // Card Row
    RowLayout {
        anchors.fill: parent
        anchors {
            fill: parent
            leftMargin: 10
            rightMargin: id_root.hasVerticalScroll ? 40 : 10

            Behavior on rightMargin {
                NumberAnimation {
                    duration: 100
                }
            }
        }
        spacing: 12

        // Icon / Logo
        Rectangle {
            width:  44
            height: 44
            radius: 4
            color: Themes.cardRow.colors.iconBackground
            Layout.alignment: Qt.AlignVCenter

            // Completed glow ring
            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: parent.radius + 1
                color: Themes.cardRow.colors.rowBackground
                border.width: id_root.isCompleted ? 1 : 0
                border.color: Themes.cardRow.colors.completedRing
                opacity: id_root.isCompleted ? 0.7 : 0.0

                Behavior on opacity {
                    NumberAnimation {
                        duration: 400
                    }
                }

                Behavior on border.width {
                    NumberAnimation {
                        duration: 400
                    }
                }
            }

            Image {
                id: id_logoImage

                anchors.fill: parent
                anchors.margins: 3
                source: id_root.p_logoSource !== "" ? id_root.p_logoSource : id_root.p_coverSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                asynchronous: true

                CustomBusyIndicator {
                    anchors.centerIn: parent
                    visible: running
                    indicatorSize: 32
                    running: id_logoImage.status === Image.Loading
                }

                // Show first letter of the title if icon/logo missing
                Rectangle {
                    anchors.fill: parent
                    radius: parent.parent.radius
                    color: Themes.cardRow.colors.fallbackBackground
                    visible: id_logoImage.status === Image.Error || id_root.p_logoSource === "" && id_root.p_coverSource === ""

                    Text {
                        anchors.centerIn: parent
                        text: id_root.p_title.length > 0 ? id_root.p_title.charAt(0).toUpperCase() : "?"
                        color: Themes.cardRow.colors.fallbackText
                        font.pixelSize: Themes.cardRow.fontSizes.fallbackText
                        font.bold: true
                    }
                }
            }
        }

        // Text block & Progress bar
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 4

            // Title & Achievement fraction
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: id_root.p_title
                    color: Themes.cardRow.colors.titleText
                    font.pixelSize: Themes.cardRow.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 4
                    visible: id_root.p_achievementTotal > 0

                    Text {
                        text: id_root.p_achievementCount + " / " + id_root.p_achievementTotal
                        color: id_root.isCompleted ? Themes.cardRow.colors.completedText : Themes.cardRow.colors.fractionText
                        font.pixelSize: Themes.cardRow.fontSizes.fraction
                        font.bold: id_root.isCompleted

                        Behavior on color {
                            ColorAnimation {
                                duration: 400
                            }
                        }
                    }

                    // TODO Replace star with subtle calm animated icon
                    Text {
                        text: "★"
                        color: Themes.cardRow.colors.star
                        font.pixelSize: Themes.cardRow.fontSizes.star
                        visible: id_root.isCompleted
                        opacity: id_root.isCompleted ? 1.0 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 400
                            }
                        }
                    }
                }
            }

            // Last played and recent unlock metadata
            RowLayout { 
                Text {
                    text: id_root.p_lastPlayed !== "" ? "🎮 " + id_root.p_lastPlayed : "🎮 " + qsTr("Never")
                    color: Themes.cardRow.colors.lastPlayed
                    font.pixelSize: Themes.cardRow.fontSizes.lastPlayed
                    elide: Text.ElideRight
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: id_root.p_recentUnlock !== "" ? qsTr("Recent unlock ") + id_root.p_recentUnlock : ""
                    color: Themes.cardRow.colors.recentUnlock
                    font.pixelSize: Themes.cardRow.fontSizes.recentUnlock
                    elide: Text.ElideRight
                }
            }

            // Progress bar
            Rectangle {
                Layout.fillWidth: true
                height: 3
                radius: 2
                color: Themes.cardRow.colors.achievementsProgressTrack
                visible: id_root.p_achievementTotal > 0

                Rectangle {
                    width: parent.width * id_root.animatedProgress
                    height: parent.height
                    radius: parent.radius
                    color: Themes.cardRow.colors.achievementsProgressFill
                }
            }
        }

        // Installation status indicator
        Rectangle {
            visible: id_root.p_status !== ""
            Layout.alignment: Qt.AlignVCenter
            width:  id_installStatusText.implicitWidth + 14
            height: 20
            radius: 10
            color: id_root.p_status === "Installed" ? Themes.cardRow.colors.installationStatusBackgroundInstalled : Themes.cardRow.colors.installationStatusBackgroundDefault

            Text {
                id: id_installStatusText

                anchors.centerIn: parent
                text: id_root.p_status
                color: id_root.p_status === "Installed" ? Themes.cardRow.colors.installationStatusTextInstalled : Themes.cardRow.colors.installationStatusTextNotInstalled
                font.pixelSize: Themes.cardRow.fontSizes.status
                font.bold: true
            }
        }
    }
}
