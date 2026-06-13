/////////////////////////////////////////////////////////
// File: CustomSwitch.qml
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom Switch
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Templates as T

T.Switch {
    id: id_root

    // Internals _____________________________________________
    readonly property int trackWidth: 36
    readonly property int trackHeight: 18
    readonly property int handleSize: 20
    readonly property int handleInset: -1
    readonly property int contentSpacing: 8

    readonly property color textColor: id_root.enabled
        ? Themes.customSwitch.colors.text
        : Themes.customSwitch.colors.textDisabled

    implicitWidth: id_label.implicitWidth
    implicitHeight: Math.max(id_indicator.implicitHeight, id_label.implicitHeight)
    spacing: contentSpacing

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Track
    indicator: Rectangle {
        id: id_indicator

        implicitWidth: id_root.trackWidth
        implicitHeight: id_root.trackHeight
        x: id_root.leftPadding
        y: id_root.topPadding + (id_root.availableHeight - height) / 2
        radius: height / 2
        color: !id_root.enabled
            ? Themes.customSwitch.colors.trackDisabled
            : (id_root.checked ? Themes.customSwitch.colors.trackOn : Themes.customSwitch.colors.trackOff)
        opacity: id_root.enabled ? 1.0 : 0.65
        border.width: id_root.checked ? 0 : 1
        border.color: Themes.customSwitch.colors.trackBorder

        Behavior on color {
            ColorAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }

        // Handle shadow
        Rectangle {
            id: id_handleShadow

            width: id_root.handleSize
            height: id_root.handleSize
            x: id_handle.x
            y: id_handle.y + 1
            radius: width / 2
            color: Themes.customSwitch.colors.handleShadow
            opacity: id_root.enabled ? (Themes.isLight ? 0.22 : 0.28) : 0.10
        }

        // Handle
        Rectangle {
            id: id_handle

            width: id_root.handleSize
            height: id_root.handleSize
            x: id_root.checked
                ? id_indicator.width - width - id_root.handleInset
                : id_root.handleInset
            y: (id_indicator.height - height) / 2
            radius: width / 2
            color: !id_root.enabled
                ? Themes.customSwitch.colors.handleDisabled
                : (id_root.checked
                    ? Themes.customSwitch.colors.handleOn
                    : Themes.customSwitch.colors.handleOff)
            border.width: 1
            border.color: Themes.customSwitch.colors.handleBorder

            Behavior on x {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    // Label
    contentItem: Text {
        id: id_label

        text: id_root.text
        color: id_root.textColor
        font.pixelSize: Themes.customSwitch.fontSizes.text
        verticalAlignment: Text.AlignVCenter
        leftPadding: id_root.indicator.width + id_root.spacing
    }
}
