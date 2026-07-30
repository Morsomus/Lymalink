/////////////////////////////////////////////////////////
// File: CustomScrollBar.qml
// Date: 2026-07-30
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom ScrollBar
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

T.ScrollBar {
    id: id_root

    // Public ________________________________________________
    property int p_minimumLength: 32
    property int p_leftMargin: 0
    property int p_rightMargin: 0
    property int p_dividerSide: Qt.LeftEdge
    
    // Internals _____________________________________________
    readonly property int thickness: 12
    readonly property int radius: 5
    readonly property int inset: 2
    readonly property int dividerThickness: 1
    readonly property bool isVertical: orientation === Qt.Vertical
    readonly property bool shouldShow: policy === ScrollBar.AlwaysOn || size < 1.0
    readonly property bool visualActive: active || hovered || pressed
    readonly property bool showLeftDivider: isVertical && p_dividerSide === Qt.LeftEdge
    readonly property bool showRightDivider: isVertical && p_dividerSide === Qt.RightEdge
    readonly property int dividerPadding: 5
    readonly property int dividerSpace: dividerThickness + dividerPadding
    readonly property int horizontalMargins: p_leftMargin + p_rightMargin
    readonly property int trackOffset: showLeftDivider ? p_leftMargin + dividerSpace : p_leftMargin

    implicitWidth: isVertical ? thickness + dividerSpace + horizontalMargins : 100
    implicitHeight: isVertical ? 100 : thickness
    topPadding: inset
    bottomPadding: inset
    leftPadding: inset + (isVertical ? trackOffset : 0)
    rightPadding: inset + (isVertical ? Math.max(0, width - trackOffset - thickness) : 0)
    minimumSize: id_root.isVertical
        ? Math.min(1.0, p_minimumLength / Math.max(1, height))
        : Math.min(1.0, p_minimumLength / Math.max(1, width))
    hoverEnabled: true
    interactive: true

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Thumb
    contentItem: Rectangle {
        id: id_thumb

        implicitWidth: id_root.isVertical
            ? id_root.thickness - id_root.inset * 2
            : id_root.p_minimumLength
        implicitHeight: id_root.isVertical
            ? id_root.p_minimumLength
            : id_root.thickness - id_root.inset * 2
        radius: id_root.radius
        color: id_root.pressed
            ? Themes.customScrollBar.colors.thumbPressed
            : (id_root.hovered || id_root.active
                ? Themes.customScrollBar.colors.thumbHover
                : Themes.customScrollBar.colors.thumb)
        visible: id_root.policy !== ScrollBar.AlwaysOff
        opacity: id_root.shouldShow ? (id_root.visualActive ? 0.95 : 0.62) : 0.0

        Behavior on color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }

    // Track
    background: Item {
        id: id_background

        implicitWidth: id_root.isVertical ? id_root.thickness + id_root.dividerSpace + id_root.horizontalMargins : 100
        implicitHeight: id_root.isVertical ? 100 : id_root.thickness
        visible: id_root.policy !== ScrollBar.AlwaysOff

        Rectangle {
            id: id_track

            x: id_root.isVertical ? id_root.trackOffset : 0
            y: 0
            width: id_root.isVertical ? id_root.thickness : parent.width
            height: parent.height
            radius: id_root.radius
            color: id_root.hovered || id_root.pressed
                ? Themes.customScrollBar.colors.trackHover
                : Themes.customScrollBar.colors.track
            opacity: id_root.shouldShow && id_root.visualActive ? 0.72 : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
        }

        Rectangle {
            x: id_root.showRightDivider
                ? id_root.p_leftMargin + id_root.thickness + id_root.dividerPadding
                : id_root.p_leftMargin
            width: Math.max(1, id_root.dividerThickness)
            height: parent.height
            color: Themes.customScrollBar.colors.trackHover
            visible: id_root.shouldShow && (id_root.showLeftDivider || id_root.showRightDivider)
        }
    }
}
