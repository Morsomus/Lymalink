/////////////////////////////////////////////////////////
// File: Card.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Card QML Component for displaying 
//              a tracked achievement as a default sized card.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes

Rectangle {
    id: id_root

    // Public ________________________________________________
    property string p_title: "Title"
    property string p_coverSource: ""
    property int p_achievementCount: 0
    property int p_achievementTotal: 0
    property string p_installationStatus: ""
    property string p_lastPlayed: ""
    property string p_recentUnlock: ""
    property bool p_miniAchievementsBadgeEnabled: false
    property bool p_edgeProgressFrameEnabled: false
    property int p_edgeProgressFrameColorStyle: 1
    property bool p_edgeProgressFrameStaticGrayColor: false
    property bool p_edgeProgressFrameCompletionAnimation: false

    signal openTargetDetails(string title, string coverSource, int achievementCount, int achievementTotal, string status, string lastPlayed, string recentUnlock)
    
    // Internals _____________________________________________
    readonly property real edgeProgressFrameCompletion: p_achievementTotal > 0 ? p_achievementCount / p_achievementTotal : 0.0

    width: 200
    height: 300
    radius: 8
    clip: true
    color: Themes.card.colors.cardBackground

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////
    
    // Cover Image / Placeholder
    Rectangle {
        anchors.fill: parent
        radius: id_root.radius
        color: Themes.card.colors.cover

        Image {
            id: id_coverImage

            z: id_errorImage.errorActive ? 2 : 0
            anchors.fill: parent
            source: id_root.p_coverSource
            fillMode: Image.PreserveAspectCrop
            smooth: true
            asynchronous: true

            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: id_coverMask
            }

            CustomBusyIndicator {
                anchors.centerIn: parent
                visible: running
                indicatorSize: 64
                running: id_coverImage.status === Image.Loading
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: id_coverImage.status === Image.Error

                ErrorImage {
                    id: id_errorImage

                    property bool errorActive: id_coverImage.status === Image.Error
                    
                    size: 128
                    visible: id_coverImage.status === Image.Error
                }
            }
        }

        // Clipping mask for rounded corners
        Rectangle {
            id: id_coverMask

            anchors.fill: parent
            radius: id_root.radius
            color: Themes.card.colors.maskFill
            visible: false
            layer.enabled: true
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 16
            visible: id_coverImage.status !== Image.Ready
            text: id_root.p_title
            color: Themes.card.colors.titleFallback
            font.pixelSize: Themes.card.fontSizes.titleFallback
            font.bold: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Edge Progress Frame
    // Draws a clockwise-filling arc along the card outline using a dash-gap trick on a rounded rect SVG path.
    Item {
        id: id_edgeProgressFrame
        
        z: 3
        anchors.fill: parent
        visible: id_root.p_edgeProgressFrameEnabled && id_root.p_achievementTotal > 0

        // Lerps from neutral grey to vivid color as progress increases
        readonly property color incompleteColor: Themes.globalStyle.progressBlendColor(
            id_root.p_edgeProgressFrameColorStyle,
            id_root.edgeProgressFrameCompletion
        )

        // Breathes between two color tones when complete - only runs when animation is enabled
        property color breathingColor: Themes.globalStyle.edgeFrameAnimAColor(id_root.p_edgeProgressFrameColorStyle)
        SequentialAnimation {
            id: id_breathingAnim
            running: id_root.edgeProgressFrameCompletion >= 1.0
                && id_root.p_edgeProgressFrameCompletionAnimation
                && !id_root.p_edgeProgressFrameStaticGrayColor
            loops: Animation.Infinite

            ColorAnimation {
                target: id_edgeProgressFrame
                property: "breathingColor"
                to: Themes.globalStyle.edgeFrameAnimBColor(id_root.p_edgeProgressFrameColorStyle)
                duration: 1500
                easing.type: Easing.InOutSine
            }
            ColorAnimation {
                target: id_edgeProgressFrame
                property: "breathingColor"
                to: Themes.globalStyle.edgeFrameAnimAColor(id_root.p_edgeProgressFrameColorStyle)
                duration: 3000
                easing.type: Easing.InOutSine
            }
        }

        readonly property color activeColor: id_root.p_edgeProgressFrameStaticGrayColor
            ? Themes.globalStyle.colors.edgeFrameGray
            : id_root.edgeProgressFrameCompletion >= 1.0
                ? (id_root.p_edgeProgressFrameCompletionAnimation
                    ? breathingColor
                    : Themes.globalStyle.completionColor(id_root.p_edgeProgressFrameColorStyle))
                : incompleteColor

        // Geometry
        readonly property int edgeProgressFrameStroke: id_root.edgeProgressFrameCompletion >= 1.0 ? 4 : 3
        readonly property real r: id_root.radius
        readonly property real w: id_root.width
        readonly property real h: id_root.height

        // Perimeter = two straight pairs + corner circles (2πr)
        readonly property real perimeter: 2 * (w - 2 * r) + 2 * (h - 2 * r) + 2 * Math.PI * r
        readonly property real dashLength: perimeter * id_root.edgeProgressFrameCompletion
        readonly property real gapLength: perimeter * (1.0 - id_root.edgeProgressFrameCompletion)

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
                strokeColor: Themes.globalStyle.colors.edgeFrameBack
                strokeWidth: id_edgeProgressFrame.edgeProgressFrameStroke + 2
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                strokeStyle: ShapePath.DashLine
                dashPattern: [
                    id_edgeProgressFrame.dashLength / strokeWidth,
                    id_edgeProgressFrame.gapLength / strokeWidth
                ]
                PathSvg {
                    path: id_edgeProgressFrame.roundedRectPath
                }
            }

            // Colored progress stroke
            ShapePath {
                strokeColor: id_edgeProgressFrame.activeColor
                strokeWidth: id_edgeProgressFrame.edgeProgressFrameStroke
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                strokeStyle: ShapePath.DashLine
                dashPattern: [
                    id_edgeProgressFrame.dashLength / strokeWidth,
                    id_edgeProgressFrame.gapLength / strokeWidth
                ]
                PathSvg {
                    path: id_edgeProgressFrame.roundedRectPath
                }
            }
        }
    }

    // Hover Overlay
    Rectangle {
        property real gradientCoverage: 0.90

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: id_root.height * gradientCoverage
        radius: id_root.radius
        color: Themes.card.colors.rootHoverOverlay
        opacity: id_rootMouseArea.containsMouse ? 1.0 : 0.0

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
                    color: Themes.card.colors.rootHoverGradientStart
                }

                GradientStop {
                    position: 1.0
                    color: Themes.card.colors.rootHoverGradientEnd
                }
            }
        }

        Column {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: 12
            }
            spacing: 3

            Text {
                text: id_root.p_title
                color: Themes.card.colors.hoverTitle
                font.pixelSize: Themes.card.fontSizes.hoverTitle
                font.bold: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                width: parent.width
            }
            Text {
                text: "🎮 " + id_root.p_lastPlayed
                color: Themes.card.colors.hoverLastPlayed
                font.pixelSize: Themes.card.fontSizes.hoverMeta
                visible: id_root.p_lastPlayed !== ""
            }
            Text {
                text: id_root.p_achievementTotal > 0 ? id_root.p_achievementCount + " / " + id_root.p_achievementTotal + " " + qsTr("Achievements") : ""
                color: Themes.card.colors.hoverAchievements
                font.pixelSize: Themes.card.fontSizes.hoverMeta
                visible: id_root.p_achievementTotal > 0
            }
        }
    }

    // Mini Achievement Badge (top-right)
    Item {
        id: id_achievementsBadge

        anchors {
            top: parent.top
            right: parent.right
        }
        width: id_achievementsBadgeText.implicitWidth + 8
        height: id_achievementsBadgeText.implicitHeight + 18
        opacity: id_root.p_miniAchievementsBadgeEnabled && id_root.p_achievementTotal > 0 && !id_rootMouseArea.containsMouse ? 1.0 : 0.0
        
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        // Flag gradient
        Rectangle {
            id: id_achievementsBadgeGradient
            
            anchors {
                top: parent.top
                bottom: parent.bottom
                right: parent.right
                bottomMargin: 5
            }
            width: id_achievementsBadge.width + 20
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop {
                    position: 0.0
                    color: Themes.card.colors.achievementsBadgeGradientStart
                }

                GradientStop {
                    position: 1.0
                    color: Themes.card.colors.achievementsBadgeGradientEnd
                }
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: id_achievementsBadgeGradientMask
            }
        }

        // Mask with top-right radius matching the card
        Rectangle {
            id: id_achievementsBadgeGradientMask
            
            anchors.fill: id_achievementsBadgeGradient
            color: "white"
            visible: false
            layer.enabled: true
            topRightRadius: id_root.radius
        }

        Text {
            id: id_achievementsBadgeText

            anchors {
                top: parent.top
                right: parent.right
                topMargin: id_root.p_edgeProgressFrameEnabled ? 8 : 7
                rightMargin: 8
            }
            text: id_root.p_achievementCount + "/" + id_root.p_achievementTotal
            color: Themes.card.colors.achievementsBadgeText
            font.pixelSize: Themes.card.fontSizes.achievementsBadge
            font.bold: true
        }
    }

    // Uninstalled Status Badge (top-left)
    Item {
        id: id_installStatusBadgeContainer

        z: 2
        anchors.fill: parent
        visible: id_root.p_installationStatus === "Not Installed" && ctxSettings.showInstallationStatusBadge

        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: id_installStatusBadgeContainerMask
        }

        Rectangle {
            id: id_installStatusBadgeBackground

            clip: true
            anchors {
                horizontalCenter: parent.left
                horizontalCenterOffset: 6
                
                verticalCenter: parent.top
                verticalCenterOffset: 6
            }
            width: 64
            height: 64
            radius: width / 2
            color: Themes.card.colors.installationStatusBadgeBackground
            opacity: Themes.card.opacity.statusBadge

            // Tooltip
            HoverHandler {
                id: id_installStatusBadgeHoverHandler
            }
        }

        Image {
            anchors {
                top: id_installStatusBadgeBackground.top
                left: id_installStatusBadgeBackground.left
                topMargin: id_installStatusBadgeBackground.width / 2 - 1
                leftMargin: id_installStatusBadgeBackground.height / 2 - 1
            }
            width: 22
            height: 22
            source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00005_2_ED.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: Themes.card.opacity.statusIcon

            CustomTooltip {
                active: id_installStatusBadgeHoverHandler.hovered
                delay: 300
                text: qsTr("Installation not found")
            }
        }

        // Mask using full card shape
        Rectangle {
            id: id_installStatusBadgeContainerMask
            
            anchors.fill: parent
            color: "white"
            visible: false
            layer.enabled: true
            radius: id_root.radius
        }
    }
    
    // Mouse Area for hover
    MouseArea {
        id: id_rootMouseArea

        z: 1
        anchors.fill: parent
        hoverEnabled: true

        onClicked: {
            id_root.openTargetDetails(
                id_root.p_title,
                id_root.p_coverSource,
                id_root.p_achievementCount,
                id_root.p_achievementTotal,
                id_root.p_installationStatus,
                id_root.p_lastPlayed,
                id_root.p_recentUnlock
            )
        }
    }

    // Lift animation
    transform: Translate {
        y: id_rootMouseArea.containsMouse && !id_rootMouseArea.pressed
            ? -4
            : id_rootMouseArea.pressed
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
        shadowColor: Themes.card.colors.rootDropshadow
        shadowOpacity: id_rootMouseArea.containsMouse ? Themes.card.opacity.shadowHover : Themes.card.opacity.shadowIdle
        shadowBlur: 1.0
        shadowVerticalOffset: id_rootMouseArea.containsMouse ? 8 : 0
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
