/////////////////////////////////////////////////////////
// File: CardRowDetailed.qml
// Date: 2026-05-11
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Detailed row card for Dashboard list view.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: id_root

    // Public ________________________________________________
    property int p_appId: 0
    property string p_title: "Title"
    property string p_coverSource: ""
    property int p_achievementCount: 0
    property int p_achievementTotal: 0
    property string p_targetType: ""
    property string p_installationStatus: ""
    property string p_lastPlayed: ""
    property string p_recentUnlock: ""
    property string p_lastAchievementIcon: ""
    property string p_lastAchievementName: ""
    property string p_lastAchievementDesc: ""
    property bool p_isLoading: false
    property int p_delegateIndex: 0
    property int p_globalColorStyle: 1

    signal openTargetDetails(int appId)

    // Internals _____________________________________________
    readonly property real progress: p_achievementTotal > 0 ? p_achievementCount / p_achievementTotal : 0.0
    readonly property bool isCompleted: p_achievementTotal > 0 && p_achievementCount >= p_achievementTotal
    readonly property bool hasVerticalScroll: ListView.view ? ListView.view.contentHeight > ListView.view.height : false
    readonly property int progressDelay: Math.max(0, Math.min(p_delegateIndex * 40, 300))
    readonly property int coverHeight: 120
    readonly property int coverWidth: Math.round(coverHeight * (2 / 3))
    readonly property color themedProgressColor: Themes.globalStyle.progressColor(p_globalColorStyle)
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(p_globalColorStyle)
    property real animatedProgress: 0.0

    height: coverHeight + 16
    clip: true
    color: Themes.cardRowDetailed.colors.rowBackground

    SequentialAnimation {
        id: id_progressAnim

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
        if (!id_progressAnim.running) {
            restartProgressAnimation()
        }
    }

    Component.onCompleted: {
        Qt.callLater(restartProgressAnimation)
    } 

    function restartProgressAnimation() {
        id_progressAnim.stop()
        animatedProgress = 0.0
        id_progressAnim.restart()
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Mouse Area for hover
    MouseArea {
        id: id_rootMouseArea

        anchors.fill: parent
        enabled: !id_root.p_isLoading
        hoverEnabled: true

        onClicked: {
            id_root.openTargetDetails(id_root.p_appId)
        }

        // Card Row Hover
        Rectangle {
            anchors {
                fill: parent
                rightMargin: id_root.hasVerticalScroll ? 30 : 0
            }
            color: id_rootMouseArea.pressed
                ? Themes.cardRowDetailed.colors.rowBackgroundPressed
                : (id_rootMouseArea.containsMouse
                    ? Themes.cardRowDetailed.colors.rowBackgroundHover
                    : Themes.cardRowDetailed.colors.rowBackground)
            // Hover border highlight
            border.width: id_rootMouseArea.containsMouse || id_rootMouseArea.pressed ? 1 : 0
            border.color: Themes.cardRowDetailed.colors.rowBorder
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

        // Cover & Completed glow ring
        Item {
            width: id_root.coverWidth
            height: id_root.coverHeight
            Layout.alignment: Qt.AlignVCenter

            // Completed glow ring - outside the clipped cover
            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                radius: 2
                color: "transparent"
                border.width: 2
                border.color: id_root.themedCompletionColor
                opacity: id_root.isCompleted ? 0.7 : 0.0
                z: 1

                Behavior on opacity {
                    NumberAnimation {
                        duration: 400
                    }
                }
            }

            // Cover
            Rectangle {
                anchors.fill: parent
                radius: 0
                color: Themes.cardRowDetailed.colors.iconBackground
                clip: true

                Image {
                    id: id_coverImage

                    anchors.fill: parent
                    source: id_root.p_coverSource
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true

                    Rectangle {
                        anchors.fill: parent
                        color: Themes.cardRowDetailed.colors.loadingOverlay
                        opacity: 0.55
                        visible: id_root.p_isLoading
                    }

                    CustomBusyIndicator {
                        anchors.centerIn: parent
                        z: 2
                        visible: p_running
                        p_indicatorSize: 48
                        p_running: id_coverImage.status === Image.Loading || id_root.p_isLoading
                        opacity: 0.6
                    }

                    // Show first letter of the title if icon/logo missing
                    Rectangle {
                        anchors.fill: parent
                        color: Themes.cardRowDetailed.colors.fallbackBackground
                        visible: !id_root.p_isLoading && (id_coverImage.status === Image.Error || id_root.p_coverSource === "")

                        Text {
                            anchors.centerIn: parent
                            text: id_root.p_title.length > 0 ? id_root.p_title.charAt(0).toUpperCase() : "?"
                            color: Themes.cardRowDetailed.colors.fallbackText
                            font.pixelSize: Themes.cardRowDetailed.fontSizes.fallbackText
                            font.bold: true
                        }
                    }
                }
            }
        }

        // Title + Status
        ColumnLayout {
            Layout.preferredWidth: 180
            Layout.fillWidth: false
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 6

            Item {
                Layout.fillHeight: true
            }

            Text {
                Layout.fillWidth: true
                text: id_root.p_title
                color: Themes.cardRowDetailed.colors.titleText
                font.pixelSize: Themes.cardRowDetailed.fontSizes.title + 2
                font.bold: true
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
                maximumLineCount: 3
            }

            RowLayout {
                visible: id_root.p_targetType !== "" || id_root.p_installationStatus !== ""
                spacing: 6

                Rectangle {
                    visible: id_root.p_installationStatus !== ""
                    implicitWidth: id_statusText.implicitWidth + 14
                    implicitHeight: 20
                    radius: 10
                    color: "transparent"
                    border.width: 1
                    border.color: id_statusText.color

                    Text {
                        id: id_statusText

                        anchors.centerIn: parent
                        text: id_root.p_installationStatus
                        color: id_root.p_installationStatus === "Installed"
                            ? id_root.themedCompletionColor
                            : Themes.cardRowDetailed.colors.installationStatusTextNotInstalled
                        font.pixelSize: Themes.cardRowDetailed.fontSizes.status
                        font.bold: true
                    }
                }

                Rectangle {
                    visible: id_root.p_targetType !== ""
                    implicitWidth: id_targetTypeText.implicitWidth + 14
                    implicitHeight: 20
                    radius: 10
                    color: "transparent"
                    border.width: 1
                    border.color: id_targetTypeText.color

                    Text {
                        id: id_targetTypeText

                        anchors.centerIn: parent
                        text: id_root.p_targetType
                        color: id_root.themedProgressColor
                        font.pixelSize: Themes.cardRowDetailed.fontSizes.status
                        font.bold: true
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // Divider
        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            color: Themes.cardRowDetailed.colors.rowBorder
        }

        // Achievements
        ColumnLayout {
            Layout.preferredWidth: 200
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 6

            Item {
                Layout.fillHeight: true
                Layout.fillWidth: id_root.p_achievementTotal > 0 ? false : true
            }

            Text {
                visible: id_root.p_achievementTotal > 0
                text: qsTr("Achievements")
                color: Themes.cardRowDetailed.colors.fractionText
                font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
            }

            // Achievement fraction
            RowLayout {
                visible: id_root.p_achievementTotal > 0
                spacing: 6

                Text {
                    text: id_root.p_achievementCount + " / " + id_root.p_achievementTotal
                    color: id_root.isCompleted
                        ? id_root.themedCompletionColor
                        : Themes.cardRowDetailed.colors.titleText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.title
                    font.bold: true

                    Behavior on color {
                        ColorAnimation {
                            duration: 400
                        }
                    }
                }

                // TODO: Replace star with subtle calm animated icon
                Text {
                    text: "★"
                    color: id_root.themedCompletionColor
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.star
                    opacity: id_root.isCompleted ? 1.0 : 0.0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 400
                        }
                    }
                }
            }

            // Progress bar
            RowLayout {
                visible: id_root.p_achievementTotal > 0
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: 3
                    color: Themes.cardRowDetailed.colors.achievementsProgressTrack

                    Rectangle {
                        width: parent.width * id_root.animatedProgress
                        height: parent.height
                        radius: parent.radius
                        color: id_root.isCompleted ? id_root.themedCompletionColor : id_root.themedProgressColor
                    }
                }

                Text {
                    text: Math.round(id_root.animatedProgress * 100) + "%"
                    color: id_root.isCompleted
                        ? id_root.themedCompletionColor
                        : Themes.cardRowDetailed.colors.fractionText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.fraction
                    font.bold: id_root.isCompleted

                    Behavior on color {
                        ColorAnimation {
                            duration: 400
                        }
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // Divider
        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            color: Themes.cardRowDetailed.colors.rowBorder
        }

        // Last Achievement
        RowLayout {
            visible: id_root.p_lastAchievementName !== ""
            Layout.preferredWidth: 240
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.rightMargin: 16
            spacing: 12

            Item {
                Layout.fillHeight: true
            }

            // Achievement Icon
            Rectangle {
                width:  64
                height: 64
                radius: 4
                color: Themes.cardRowDetailed.colors.iconBackground
                Layout.alignment: Qt.AlignVCenter
                clip: true

                Image {
                    id: id_achievementIcon

                    anchors.fill: parent
                    source: id_root.p_lastAchievementIcon
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                // Fallback: first letter of achievement name
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: Themes.cardRowDetailed.colors.fallbackBackground
                    visible: id_achievementIcon.status !== Image.Ready || id_root.p_lastAchievementIcon === ""

                    Text {
                        anchors.centerIn: parent
                        text: id_root.p_lastAchievementName.length > 0
                            ? id_root.p_lastAchievementName.charAt(0).toUpperCase()
                            : "?"
                        color: Themes.cardRowDetailed.colors.fallbackText
                        font.pixelSize: Themes.cardRowDetailed.fontSizes.fallbackText
                        font.bold: true
                    }
                }
            }

            // Achievement Name + description
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 3

                Text {
                    text: qsTr("Recent Achievement")
                    color: Themes.cardRowDetailed.colors.fractionText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
                }

                Text {
                    Layout.fillWidth: true
                    text: id_root.p_lastAchievementName
                    color: Themes.cardRowDetailed.colors.titleText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.lastPlayed
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: id_root.p_lastAchievementDesc
                    color: Themes.cardRowDetailed.colors.fractionText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        // Divider - only shown when there is a last achievement to display
        Rectangle {
            visible: id_root.p_lastAchievementName !== ""
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            color: Themes.cardRowDetailed.colors.rowBorder
        }

        // Activity
        ColumnLayout {
            Layout.preferredWidth: 130
            Layout.fillWidth: false
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 6

            Item {
                Layout.fillHeight: true
            }

            ColumnLayout {
                spacing: 2

                Text {
                    text: qsTr("Last Played")
                    color: Themes.cardRowDetailed.colors.fractionText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
                }

                Text {
                    text: id_root.p_lastPlayed !== "" ? id_root.p_lastPlayed : qsTr("Never")
                    color: Themes.cardRowDetailed.colors.titleText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.lastPlayed
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: 2
                visible: id_root.p_recentUnlock !== ""

                Text {
                    text: qsTr("Recent Unlock")
                    color: Themes.cardRowDetailed.colors.fractionText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
                }

                Text {
                    text: id_root.p_recentUnlock
                    color: Themes.cardRowDetailed.colors.titleText
                    font.pixelSize: Themes.cardRowDetailed.fontSizes.recentUnlock
                    font.bold: true
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
