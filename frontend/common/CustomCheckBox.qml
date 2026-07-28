/////////////////////////////////////////////////////////
// File: CustomCheckBox.qml
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom CheckBox
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Templates as T

T.CheckBox {
    id: id_root

    // Internals _____________________________________________
    readonly property int boxSize: 18
    readonly property int contentSpacing: 8
    readonly property int labelSpacing: id_root.text.length > 0 ? id_root.spacing : 0
    property bool labelOnLeft: false
    readonly property bool partiallyChecked: id_root.checkState === Qt.PartiallyChecked
    readonly property color boxColor: !id_root.enabled
        ? Themes.customCheckBox.colors.backgroundDisabled
        : (id_root.down
            ? Themes.customCheckBox.colors.backgroundPressed
            : (id_root.hovered
                ? Themes.customCheckBox.colors.backgroundHover
                : Themes.customCheckBox.colors.background))
    readonly property color borderColor: id_root.visualFocus
        ? Themes.customCheckBox.colors.borderFocus
        : (id_root.checked || id_root.partiallyChecked
            ? Themes.customCheckBox.colors.borderChecked
            : (id_root.hovered && id_root.enabled
                ? Themes.customCheckBox.colors.borderHover
                : Themes.customCheckBox.colors.border))
    readonly property color markColor: id_root.enabled
        ? Themes.customCheckBox.colors.mark
        : Themes.customCheckBox.colors.markDisabled
    readonly property color textColor: id_root.enabled
        ? Themes.customCheckBox.colors.text
        : Themes.customCheckBox.colors.textDisabled

    implicitWidth: id_indicator.implicitWidth + id_root.labelSpacing + id_label.implicitWidth
    implicitHeight: Math.max(id_indicator.implicitHeight, id_label.implicitHeight)
    spacing: contentSpacing

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Box
    indicator: Rectangle {
        id: id_indicator

        implicitWidth: id_root.boxSize
        implicitHeight: id_root.boxSize
        x: id_root.labelOnLeft ? id_root.width - id_root.rightPadding - width : id_root.leftPadding
        y: id_root.topPadding + (id_root.availableHeight - height) / 2
        radius: 4
        color: id_root.checked || id_root.partiallyChecked
            ? Themes.customCheckBox.colors.backgroundChecked
            : id_root.boxColor
        opacity: id_root.enabled ? 1.0 : 0.70
        border.width: 1
        border.color: id_root.borderColor

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

        // Check mark
        Item {
            visible: id_root.checked && !id_root.partiallyChecked
            anchors.fill: parent
            opacity: id_root.enabled ? 1.0 : 0.70

            Rectangle {
                width: 2
                height: 7
                x: 2
                y: 9
                radius: 1
                color: id_root.markColor
                rotation: -45
                transformOrigin: Item.Top
            }

            Rectangle {
                width: 2
                height: 11
                x: 14
                y: 5
                radius: 1
                color: id_root.markColor
                rotation: 45
                transformOrigin: Item.Top
            }
        }

        // Partial mark
        Rectangle {
            visible: id_root.partiallyChecked
            width: 10
            height: 2
            anchors.centerIn: parent
            radius: 1
            color: id_root.markColor
            opacity: id_root.enabled ? 1.0 : 0.70
        }
    }

    // Label
    contentItem: Text {
        id: id_label

        text: id_root.text
        color: id_root.textColor
        font.family: id_root.font.family
        font.bold: id_root.font.bold
        font.italic: id_root.font.italic
        font.weight: id_root.font.weight
        font.pixelSize: Themes.customCheckBox.fontSizes.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: id_root.labelOnLeft ? Text.AlignRight : Text.AlignLeft
        leftPadding: id_root.labelOnLeft ? 0 : id_root.indicator.width + id_root.labelSpacing
        rightPadding: id_root.labelOnLeft ? id_root.indicator.width + id_root.labelSpacing : 0
        elide: Text.ElideRight
    }
}
