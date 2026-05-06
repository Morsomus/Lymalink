/////////////////////////////////////////////////////////
// File: CardRow.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: CardRow QML Component for displaying 
//              a tracked achievement or target in row format.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: "Title"
    property string coverSource: ""     // Full cover image (fallback)
    property string logoSource: ""      // Prefer transparent library logo
    property int achievementCount: 0    // e.g. 5
    property int achievementTotal: 0    // e.g. 73
    property string status: ""          // "Installed" | "Not Installed"
    property string lastPlayed: ""      // e.g. "2 days ago"
    property int delegateIndex: 0

    // Internals
    readonly property real progress: achievementTotal > 0 ? achievementCount / achievementTotal : 0.0
    readonly property bool isCompleted: achievementTotal > 0 && achievementCount >= achievementTotal
    readonly property bool hasVerticalScroll: ListView.view ? ListView.view.contentHeight > ListView.view.height : false
    property real animatedProgress: 0.0

    height: 64
    // width: set by parent (ListView delegate width: listView.width)
    radius: 6
    clip: true
    color: hoverArea.pressed
           ? Themes.cardRow.colors.rowBackgroundPressed
           : (hoverArea.containsMouse
            ? Themes.cardRow.colors.rowBackgroundHover
            : Themes.cardRow.colors.rowBackground)

    // Hover border highlight
    border.width: hoverArea.containsMouse || hoverArea.pressed ? 1 : 0
    border.color: Themes.cardRow.colors.rowBorder

    function restartProgressAnimation() {
        progressIntroAnimation.stop()
        animatedProgress = 0.0
        progressIntroAnimation.restart()
    }

    Connections {
        target: root.ListView.view
        function onVisibleChanged() {
            if (root.ListView.view.visible) {
                root.restartProgressAnimation()
            }
        }
    }

    Component.onCompleted: {
        Qt.callLater(restartProgressAnimation)
    }

    SequentialAnimation {
        id: progressIntroAnimation

        PauseAnimation {
            duration: Math.min(root.delegateIndex * 40, 300)
        }
        NumberAnimation {
            target: root
            property: "animatedProgress"
            from: 0.0
            to: root.progress
            duration: 550
            easing.type: Easing.OutCubic
        }
    }

    onProgressChanged: {
        if (!progressIntroAnimation.running) {
            restartProgressAnimation()
        }
    }

    Behavior on color {
        ColorAnimation {
            duration: 120
        }
    }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: 10
            rightMargin: root.hasVerticalScroll ? 40 : 10
            topMargin: 0
            bottomMargin: 0

            Behavior on rightMargin {
                NumberAnimation {
                    duration: 100
                }
            }
        }
        spacing: 12

        // Icon / Logo
        Rectangle {
            id: iconContainer

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
                border.width: root.isCompleted ? 1 : 0
                border.color: Themes.cardRow.colors.completedRing
                opacity: root.isCompleted ? 0.7 : 0.0

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
                id: logoImage

                anchors.fill: parent
                anchors.margins: 3
                source: root.logoSource !== "" ? root.logoSource : root.coverSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                asynchronous: true

                BusyIndicator {
                    anchors.centerIn: parent
                    running: logoImage.status === Image.Loading
                    visible: running
                    width: 40
                    height: 40
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.parent.radius
                    color: Themes.cardRow.colors.fallbackBackground
                    visible: logoImage.status === Image.Error || root.logoSource === "" && root.coverSource === ""

                    Text {
                        anchors.centerIn: parent
                        text: root.title.length > 0 ? root.title.charAt(0).toUpperCase() : "?"
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
                    text: root.title
                    color: Themes.cardRow.colors.titleText
                    font.pixelSize: Themes.cardRow.fontSizes.title
                    font.bold: true
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 4
                    visible: root.achievementTotal > 0

                    Text {
                        id: fractionText
                        
                        text: root.achievementCount + " / " + root.achievementTotal
                        color: root.isCompleted ? Themes.cardRow.colors.completedText : Themes.cardRow.colors.fractionText
                        font.pixelSize: Themes.cardRow.fontSizes.fraction
                        font.bold: root.isCompleted

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
                        visible: root.isCompleted
                        opacity: root.isCompleted ? 1.0 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 400
                            }
                        }
                    }
                }
            }

            // Last played metadata
            Text {
                Layout.fillWidth: true
                text: root.lastPlayed
                color: Themes.cardRow.colors.lastPlayed
                font.pixelSize: Themes.cardRow.fontSizes.lastPlayed
                visible: root.lastPlayed !== ""
                elide: Text.ElideRight
            }

            // Progress bar
            Rectangle {
                Layout.fillWidth: true
                height: 3
                radius: 2
                color: Themes.cardRow.colors.progressTrack
                visible: root.achievementTotal > 0

                Rectangle {
                    width: parent.width * root.animatedProgress
                    height: parent.height
                    radius: parent.radius
                    color: Themes.cardRow.colors.progressFill
                }
            }
        }

        // Installation status indicator
        Rectangle {
            visible: root.status !== ""
            Layout.alignment: Qt.AlignVCenter
            width:  statusText.implicitWidth + 14
            height: 20
            radius: 10
            color: root.status === "Installed" ? Themes.cardRow.colors.statusBackgroundInstalled : Themes.cardRow.colors.statusBackgroundDefault

            Text {
                id: statusText

                anchors.centerIn: parent
                text: root.status
                color: root.status === "Installed" ? Themes.cardRow.colors.statusTextInstalled : Themes.cardRow.colors.statusTextDefault
                font.pixelSize: Themes.cardRow.fontSizes.status
                font.bold: true
            }
        }
    }

    // Hover area
    MouseArea {
        id: hoverArea

        anchors.fill: parent
        hoverEnabled: true
    }
}
