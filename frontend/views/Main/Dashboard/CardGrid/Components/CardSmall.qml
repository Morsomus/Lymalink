/////////////////////////////////////////////////////////
// File: CardSmall.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: CardSmall QML Component for displaying 
//              a tracked achievement as a medium sized card.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: root

    property string title: "Title"
    property string coverSource: ""
    property int achievementCount: 0
    property int achievementTotal: 0
    property string status: ""
    property string lastPlayed: ""

    // Progress Indicator Visibility
    property bool showEdgeProgressFrame: true
    property bool showMiniAchievementsBadge: true

    // Internals
    readonly property real progress: achievementTotal > 0 ? achievementCount / achievementTotal : 0.0
    readonly property int frameInset: root.showEdgeProgressFrame ? 2 : 0
    property real gradientCoverage: 0.90

    width: 150
    height: 225
    radius: 8
    clip: true
    color: Themes.cardSmall.colors.cardBackground

    // Cover Image / Placeholder
    Rectangle {
        id: coverPlaceholder

        anchors.fill: parent
        radius: root.radius
        color: Themes.cardSmall.colors.coverPlaceholder

        Image {
            id: coverImage

            anchors.fill: parent
            source: root.coverSource
            fillMode: Image.PreserveAspectCrop
            smooth: true
            asynchronous: true

            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: coverMask
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: coverImage.status === Image.Loading
                visible: running
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: coverImage.status === Image.Error

                // TODO: Replace with Error IMG
                Rectangle {
                    width: 28
                    height: 28
                    color: Themes.cardSmall.colors.imageErrorBlock
                    radius: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: qsTr("Image error")
                    color: Themes.cardSmall.colors.imageErrorText
                    font.pixelSize: Themes.cardSmall.fontSizes.imageError
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // Clipping mask for rounded corners
        Rectangle {
            id: coverMask

            anchors.fill: parent
            radius: root.radius
            color: Themes.cardSmall.colors.maskFill
            visible: false
            layer.enabled: true
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 16
            visible: coverImage.status !== Image.Ready
            text: root.title
            color: Themes.cardSmall.colors.titleFallback
            font.pixelSize: Themes.cardSmall.fontSizes.titleFallback
            font.bold: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Edge Progress Frame
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Themes.cardSmall.colors.edgeFrame
        visible: root.showEdgeProgressFrame
        border.width: 2
        border.color: Qt.rgba(0.2 + root.progress * 0.8, 0.8, 0.4, 0.85)
        // TODO Canvas/ShaderEffect clockwise arc + Achieved final color
    }

    // Hover Overlay
    Rectangle {
        id: hoverOverlay

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: frameInset
            rightMargin: frameInset
            bottomMargin: frameInset
        }
        height: (root.height - frameInset) * root.gradientCoverage
        radius: root.radius
        color: Themes.cardSmall.colors.hoverOverlay
        opacity: rootMouseArea.containsMouse ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        // Gradient fill
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Themes.cardSmall.colors.hoverGradientStart
                }

                GradientStop {
                    position: 1.0
                    color: Themes.cardSmall.colors.hoverGradientEnd
                }
            }
        }

        Column {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: 10
            }
            spacing: 2

            Text {
                text: root.title
                color: Themes.cardSmall.colors.hoverTitle
                font.pixelSize: Themes.cardSmall.fontSizes.hoverTitle
                font.bold: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                width: parent.width
            }
            Text {
                text: root.achievementTotal > 0 ? root.achievementCount + " / " + root.achievementTotal : ""
                color: Themes.cardSmall.colors.hoverAchievements
                font.pixelSize: Themes.cardSmall.fontSizes.hoverMeta
                visible: root.achievementTotal > 0
            }
            Text {
                text: root.lastPlayed
                color: Themes.cardSmall.colors.hoverLastPlayed
                font.pixelSize: Themes.cardSmall.fontSizes.hoverMeta
                visible: root.lastPlayed !== ""
            }
        }
    }

    // Mini Achievement Badge (top-right)
    Item {
        id: badge

        anchors {
            top: parent.top
            right: parent.right
            topMargin: frameInset
            rightMargin: frameInset
        }
        width: badgeText.implicitWidth + 6
        height: badgeText.implicitHeight + 15
        opacity: root.showMiniAchievementsBadge && root.achievementTotal > 0 && !rootMouseArea.containsMouse ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        // Flag gradient
        Rectangle {
            id: badgeGradient
            
            anchors {
                top: parent.top
                bottom: parent.bottom
                right: parent.right

                topMargin: 6
                bottomMargin: 5
            }
            width: badge.width + 20
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop {
                    position: 0.0
                    color: Themes.cardSmall.colors.badgeGradientStart
                }

                GradientStop {
                    position: 1.0
                    color: Themes.cardSmall.colors.badgeGradientEnd
                }
            }

            // layer.enabled: true
            // layer.effect: MultiEffect {
            //     maskEnabled: true
            //     maskSource: badgeGradientMask
            // }
        }

        // Mask with top-right radius matching the card
        // Rectangle {
        //     id: badgeGradientMask
        //     anchors.fill: badgeGradient
        //     color: "white"
        //     visible: false
        //     layer.enabled: true
        //     topRightRadius: root.radius
        // }

        Text {
            id: badgeText

            anchors {
                top: parent.top
                right: parent.right
                topMargin: frameInset + 6
                rightMargin: frameInset + 6
            }
            text: root.achievementCount + "/" + root.achievementTotal
            color: Themes.cardSmall.colors.badgeText
            font.pixelSize: Themes.cardSmall.fontSizes.badge
            font.bold: true
        }
    }

    // Uninstalled Status Badge (top-left)
    Item {
        z: 2
        visible: root.status === "Not Installed"

        Rectangle {
            id: statusBadgeBackground

            anchors {
                top: parent.top
                left: parent.left
                margins: 6
            }
            width: 28
            height: 28
            radius: width / 2
            color: Themes.cardSmall.colors.statusBadgeBackground
            opacity: Themes.cardSmall.opacity.statusBadge

            // Tooltip
            HoverHandler {
                id: hoverHandler
            }
            
            ToolTip.visible: hoverHandler.hovered
            ToolTip.text: qsTr("Installation not found")
            ToolTip.delay: 300
        }

        Image {
            anchors.centerIn: statusBadgeBackground
            width: 18
            height: 18
            source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00005_2_ED.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: Themes.cardSmall.opacity.statusIcon
        }
    }

    // Mouse Area for hover
    MouseArea {
        id: rootMouseArea

        z: 1
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            // Open detailed information
        }
    }

    // Lift animation
    transform: Translate {
        y: rootMouseArea.containsMouse && !rootMouseArea.pressed
            ? -4
            : rootMouseArea.pressed
                ? 3
                : 0

        Behavior on y {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutQuad
            }
        }
    }

    // Drop shadow
    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Themes.cardSmall.colors.shadow
        shadowOpacity: rootMouseArea.containsMouse ? Themes.cardSmall.opacity.shadowHover : Themes.cardSmall.opacity.shadowIdle
        shadowBlur: 1.0
        shadowVerticalOffset: rootMouseArea.containsMouse ? 8 : 0
        shadowHorizontalOffset: 0

        Behavior on shadowOpacity {
            NumberAnimation {
                duration: 150
            }
        }

        Behavior on shadowVerticalOffset {
            NumberAnimation {
                duration: 150
            }
        }
    }
}
