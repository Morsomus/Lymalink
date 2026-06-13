/////////////////////////////////////////////////////////
// File: CustomTextField.qml
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom TextField
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls as C

C.TextField {
    id: id_root

    // Internals _____________________________________________
    readonly property color backgroundColor: !id_root.enabled
        ? Themes.customTextField.colors.backgroundDisabled
        : (id_root.hovered
            ? Themes.customTextField.colors.backgroundHover
            : Themes.customTextField.colors.background)
    readonly property color borderColor: id_root.activeFocus
        ? Themes.customTextField.colors.borderFocus
        : (id_root.hovered && id_root.enabled
            ? Themes.customTextField.colors.borderHover
            : Themes.customTextField.colors.border)

    implicitWidth: 450
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    color: id_root.enabled
        ? Themes.customTextField.colors.text
        : Themes.customTextField.colors.textDisabled
    placeholderTextColor: Themes.customTextField.colors.placeholderText
    selectedTextColor: Themes.customTextField.colors.selectedText
    selectionColor: Themes.customTextField.colors.selection
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: Themes.customTextField.fontSizes.text

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Text field frame
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
