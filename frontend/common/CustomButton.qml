/////////////////////////////////////////////////////////
// File: CustomButton.qml
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom Button
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Templates as T

T.Button {
    id: id_root

    property string p_tooltipText: ""

    // Internals _____________________________________________
    readonly property color backgroundColor: !id_root.enabled
        ? Themes.customButton.colors.backgroundDisabled
        : (id_root.down
            ? Themes.customButton.colors.backgroundPressed
            : (id_root.hovered
                ? Themes.customButton.colors.backgroundHover
                : Themes.customButton.colors.background))
    readonly property color borderColor: id_root.visualFocus
        ? Themes.customButton.colors.borderFocus
        : (id_root.hovered && id_root.enabled
            ? Themes.customButton.colors.borderHover
            : Themes.customButton.colors.border)
    readonly property color textColor: id_root.enabled
        ? Themes.customButton.colors.text
        : Themes.customButton.colors.textDisabled

    implicitWidth: Math.max(80, id_label.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 30
    leftPadding: 14
    rightPadding: 14
    topPadding: 0
    bottomPadding: 0

    HoverHandler {
        id: id_buttonHover
    }

    CustomTooltip {
        p_active: id_root.p_tooltipText !== "" && id_buttonHover.hovered
        p_delay: 600
        p_text: id_root.p_tooltipText
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Button text
    contentItem: Text {
        id: id_label

        text: id_root.text
        color: id_root.textColor
        font.family: id_root.font.family
        font.bold: id_root.font.bold
        font.italic: id_root.font.italic
        font.weight: id_root.font.weight
        font.pixelSize: Themes.customButton.fontSizes.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // Button frame
    background: Rectangle {
        color: id_root.backgroundColor
        radius: 4
        border.width: 1
        border.color: id_root.borderColor
        opacity: id_root.enabled ? 1.0 : 0.70

        Behavior on color {
            ColorAnimation {
                duration: 100
                easing.type: Easing.OutCubic
            }
        }

        Behavior on border.color {
            ColorAnimation {
                duration: 100
                easing.type: Easing.OutCubic
            }
        }
    }
}
