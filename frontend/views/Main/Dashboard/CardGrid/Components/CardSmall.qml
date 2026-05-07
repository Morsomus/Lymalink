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
import QtQuick.Shapes

Rectangle {
    id: root

    property string title: "Title"
    property string coverSource: ""
    property int achievementCount: 0
    property int achievementTotal: 0
    property string status: ""
    property string lastPlayed: ""

    property bool miniAchievementsBadgeEnabled: false
    property bool edgeProgressFrameEnabled: false
    property bool edgeProgressFrameStaticGrayColor: false
    property bool edgeProgressFrameCompletionAnimation: false

    // Internals
    readonly property real edgeProgressFrameCompletion: achievementTotal > 0 ? achievementCount / achievementTotal : 0.0

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
    // Draws a clockwise-filling arc along the card outline using a dash-gap trick on a rounded rect SVG path.
    Item {
        id: edgeProgressFrame
        
        z: 2
        anchors.fill: parent
        visible: root.edgeProgressFrameEnabled && root.achievementTotal > 0

        // Colors
        readonly property color grayModeColor: Themes.card.colors.edgeFrameGray
        readonly property color completionColorA: Themes.card.colors.edgeFrameGlowA
        readonly property color completionColorB: Themes.card.colors.edgeFrameGlowB
        readonly property color completionColorStatic: Themes.card.colors.edgeFrameDone

        // Lerps from neutral grey to vivid green-gold as progress increases
        readonly property color incompleteColor: {
            const grey = 0.45
            const p = root.edgeProgressFrameCompletion
            return Qt.rgba(
                grey + p * (1.0 - grey),
                grey + p * (0.8 - grey),
                grey + p * (0.4 - grey),
                0.60 + p * 0.25
            )
        }

        // Breathes between two gold tones when complete - only runs when animation is enabled
        property color breathingColor: completionColorA
        SequentialAnimation {
            running: root.edgeProgressFrameCompletion >= 1.0 && root.edgeProgressFrameCompletionAnimation && !root.edgeProgressFrameStaticGrayColor
            loops: Animation.Infinite

            ColorAnimation {
                target: edgeProgressFrame
                property: "breathingColor"
                to: edgeProgressFrame.completionColorB
                duration: 1500
                easing.type: Easing.InOutSine
            }
            ColorAnimation {
                target: edgeProgressFrame
                property: "breathingColor"
                to: edgeProgressFrame.completionColorA
                duration: 3000
                easing.type: Easing.InOutSine
            }
        }

        readonly property color activeColor: root.edgeProgressFrameStaticGrayColor
            ? grayModeColor
            : root.edgeProgressFrameCompletion >= 1.0
                ? (root.edgeProgressFrameCompletionAnimation ? breathingColor : completionColorStatic)
                : incompleteColor

        // Geometry
        readonly property int edgeProgressFrameStroke: root.edgeProgressFrameCompletion >= 1.0 ? 4 : 3
        readonly property real r: root.radius
        readonly property real w: root.width
        readonly property real h: root.height

        // Perimeter = two straight pairs + corner circles (2πr)
        readonly property real perimeter: 2 * (w - 2 * r) + 2 * (h - 2 * r) + 2 * Math.PI * r
        readonly property real dashLength: perimeter * root.edgeProgressFrameCompletion
        readonly property real gapLength: perimeter * (1.0 - root.edgeProgressFrameCompletion)

        // Rounded rect path starting at top-left, traced clockwise
        readonly property string roundedRectPath: `M ${r},0
            L ${w - r},0
            A ${r},${r} 0 0 1 ${w},${r}
            L ${w},${h - r}
            A ${r},${r} 0 0 1 ${w - r},${h}
            L ${r},${h}
            A ${r},${r} 0 0 1 0,${h - r}
            L 0,${r}
            A ${r},${r} 0 0 1 ${r},0
            Z`

        Shape {
            anchors.fill: parent
            layer.enabled: true
            layer.samples: 4

            // Black backing stroke provides contrast separation on any background
            ShapePath {
                strokeColor: Themes.card.colors.edgeFrameBack
                strokeWidth: edgeProgressFrame.edgeProgressFrameStroke + 2
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                strokeStyle: ShapePath.DashLine
                dashPattern: [
                    edgeProgressFrame.dashLength / strokeWidth,
                    edgeProgressFrame.gapLength / strokeWidth
                ]
                PathSvg {
                    path: edgeProgressFrame.roundedRectPath
                }
            }

            // Colored progress stroke
            ShapePath {
                strokeColor: edgeProgressFrame.activeColor
                strokeWidth: edgeProgressFrame.edgeProgressFrameStroke
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                strokeStyle: ShapePath.DashLine
                dashPattern: [
                    edgeProgressFrame.dashLength / strokeWidth,
                    edgeProgressFrame.gapLength / strokeWidth
                ]
                PathSvg {
                    path: edgeProgressFrame.roundedRectPath
                }
            }
        }
    }

    // Hover Overlay
    Rectangle {
        id: hoverOverlay

        property real gradientCoverage: 0.90

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: root.height * gradientCoverage
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
        }
        width: badgeText.implicitWidth + 6
        height: badgeText.implicitHeight + 15
        opacity: root.miniAchievementsBadgeEnabled && root.achievementTotal > 0 && !rootMouseArea.containsMouse ? 1.0 : 0.0

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

            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: badgeGradientMask
            }
        }

        // Mask with top-right radius matching the card
        Rectangle {
            id: badgeGradientMask
            anchors.fill: badgeGradient
            color: "white"
            visible: false
            layer.enabled: true
            topRightRadius: root.radius
        }

        Text {
            id: badgeText

            anchors {
                top: parent.top
                right: parent.right
                topMargin: root.edgeProgressFrameEnabled ? 6 : 5
                rightMargin: 6
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
