/////////////////////////////////////////////////////////
// File: CustomComboBox.qml
// Date: 2026-06-13
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom ComboBox
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

T.ComboBox {
    id: id_root

    // Public ________________________________________________
    property int p_visibleRows: 8
    property int p_rowHeight: 32
    property var p_textFromValue: function(value, index) {
        return value
    }

    // Internals _____________________________________________
    readonly property color textColor: enabled ? Themes.customComboBox.colors.text : Themes.customComboBox.colors.textDisabled
    readonly property color indicatorColor: enabled ? Themes.customComboBox.colors.indicator : Themes.customComboBox.colors.indicatorDisabled

    implicitWidth: 150
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 28
    topPadding: 0
    bottomPadding: 0
    displayText: currentIndex >= 0 ? optionText(model[currentIndex], currentIndex) : ""

    function optionText(value, index) {
        const resolved = p_textFromValue(value, index)
        return resolved === undefined || resolved === null ? "" : resolved.toString()
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Popup row
    delegate: T.ItemDelegate {
        id: id_delegate

        required property int index
        required property var modelData

        width: id_root.width - id_root.popup.leftPadding - id_root.popup.rightPadding
        height: id_root.p_rowHeight
        highlighted: id_root.highlightedIndex === index

        contentItem: Text {
            text: id_root.optionText(id_delegate.modelData, id_delegate.index)
            leftPadding: id_root.leftPadding
            color: id_root.textColor
            font.pixelSize: Themes.customComboBox.fontSizes.rowText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: id_delegate.highlighted
                ? Themes.customComboBox.colors.rowSelected
                : (id_delegate.hovered
                    ? Themes.customComboBox.colors.rowHover
                    : "transparent")
            radius: 4
        }
    }

    // Dropdown arrow
    indicator: Item {
        x: id_root.width - width - 8
        y: (id_root.height - height) / 2 + 3
        width: 14
        height: 14

        Rectangle {
            width: 7
            height: 1
            x: 0
            y: 6
            color: id_root.indicatorColor
            rotation: 45
            transformOrigin: Item.Right
        }

        Rectangle {
            width: 7
            height: 1
            x: 6
            y: 6
            color: id_root.indicatorColor
            rotation: -45
            transformOrigin: Item.Left
        }
    }

    // Selected value
    contentItem: Text {
        // leftPadding: id_root.leftPadding
        rightPadding: id_root.rightPadding
        text: id_root.displayText
        color: id_root.textColor
        font.pixelSize: Themes.customComboBox.fontSizes.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // Closed field
    background: Rectangle {
        color: !id_root.enabled
            ? Themes.customComboBox.colors.backgroundPressed
            : (id_root.down
                ? Themes.customComboBox.colors.backgroundPressed
                : (id_root.hovered
                    ? Themes.customComboBox.colors.backgroundHover
                    : Themes.customComboBox.colors.background))
        radius: 4
        border.width: 1
        border.color: id_root.visualFocus || id_root.popup.visible
            ? Themes.customComboBox.colors.borderActive
            : Themes.customComboBox.colors.border
    }

    // Option popup
    popup: T.Popup {
        y: id_root.height + 2
        width: id_root.width
        implicitHeight: Math.min(
            id_root.p_visibleRows * id_root.p_rowHeight,
            id_popupList.contentHeight
        ) + topPadding + bottomPadding
        padding: 4
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent

        contentItem: ListView {
            id: id_popupList

            clip: true
            implicitHeight: contentHeight
            model: id_root.popup.visible ? id_root.delegateModel : null
            currentIndex: id_root.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: Themes.customComboBox.colors.popupBackground
            radius: 4
            border.width: 1
            border.color: Themes.customComboBox.colors.border
        }
    }

    // Ignore wheel changes while scrolling settings page
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) {
            event.accepted = true
        }
    }
}
