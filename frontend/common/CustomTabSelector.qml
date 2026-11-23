/////////////////////////////////////////////////////////
// File: CustomTabSelector.qml
// Date: 2026-10-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Two-option tab selector
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

T.Control {
    id: id_root

    // Public ________________________________________________
    property string p_firstText: ""
    property string p_secondText: ""
    property int p_currentIndex: 0
    property color p_accentColor: "transparent"

    signal activated(int index)

    // Internals _____________________________________________
    readonly property bool accentEnabled: enabled && p_accentColor.a > 0

    implicitWidth: 240
    implicitHeight: 40
    padding: 2

    function selectIndex(index) {
        const selectionChanged = id_root.p_currentIndex !== index
        if (!id_root.enabled || !selectionChanged) {
            return
        }

        id_root.activated(index)
    }

    background: Rectangle {
        readonly property color neutralBorder: id_root.visualFocus
            ? Themes.customTabSelector.colors.borderFocus
            : Themes.customTabSelector.colors.border

        radius: 6
        color: Themes.customTabSelector.colors.background
        border.width: 1
        border.color: id_root.accentEnabled
            ? Qt.tint(neutralBorder, Themes.globalStyle.withAlpha(id_root.p_accentColor,
                id_root.visualFocus ? Themes.statistics.opacity.activeBorder : Themes.statistics.opacity.border))
            : neutralBorder
    }

    contentItem: RowLayout {
        spacing: 2

        Repeater {
            model: [id_root.p_firstText, id_root.p_secondText]

            delegate: T.Button {
                id: id_tab

                required property int index
                required property string modelData

                readonly property bool selected: id_root.p_currentIndex === index

                Layout.fillWidth: true
                Layout.fillHeight: true
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                onClicked: id_root.selectIndex(index)

                contentItem: Text {
                    text: id_tab.modelData
                    color: id_tab.enabled
                        ? (id_tab.selected
                            ? Themes.customTabSelector.colors.textSelected
                            : Themes.customTabSelector.colors.text)
                        : Themes.customTabSelector.colors.textDisabled
                    font.pixelSize: Themes.customTabSelector.fontSizes.text
                    font.bold: id_tab.selected
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    readonly property color neutralBackground: !id_tab.enabled
                        ? Themes.customTabSelector.colors.backgroundDisabled
                        : (id_tab.down
                            ? Themes.customTabSelector.colors.backgroundPressed
                            : (id_tab.selected
                                ? Themes.customTabSelector.colors.backgroundSelected
                                : (id_tab.hovered
                                    ? Themes.customTabSelector.colors.backgroundHover
                                    : Themes.customTabSelector.colors.background)))
                    readonly property color neutralBorder: id_tab.visualFocus
                        ? Themes.customTabSelector.colors.borderFocus
                        : Themes.customTabSelector.colors.borderSelected
                    readonly property real accentOpacity: id_tab.down || id_tab.selected
                        ? Themes.statistics.opacity.selected
                        : (id_tab.hovered ? Themes.statistics.opacity.hover : 0)

                    radius: 4
                    color: id_root.accentEnabled && id_tab.enabled
                        ? Qt.tint(neutralBackground, Themes.globalStyle.withAlpha(id_root.p_accentColor, accentOpacity))
                        : neutralBackground
                    border.width: id_tab.selected || id_tab.visualFocus ? 1 : 0
                    border.color: id_root.accentEnabled && id_tab.enabled
                        ? Qt.tint(neutralBorder, Themes.globalStyle.withAlpha(id_root.p_accentColor, Themes.statistics.opacity.activeBorder))
                        : neutralBorder

                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }
}
