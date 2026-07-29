/////////////////////////////////////////////////////////
// File: DashboardSettings.qml
// Date: 2026-07-29
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Popup for Dashboard maintenance actions
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    signal missingMetadataReloadQueued(var targets)

    width: Math.min(340, parent ? parent.width - 48 : 340)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.close()
    }

    Overlay.modal: Rectangle {
        color: Themes.targetSettings.colors.overlay
    }

    background: Rectangle {
        radius: 8
        color: Themes.targetSettings.colors.background
        border.width: 1
        border.color: Themes.targetSettings.colors.border
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_ActionButton: CustomButton {
        id: id_button

        property string tooltipText: ""
        property bool danger: false

        Layout.fillWidth: true
        implicitHeight: 40

        contentItem: Label {
            text: id_button.text
            color: id_button.danger
                ? Themes.targetSettings.colors.dangerText
                : Themes.targetSettings.colors.buttonText
            font.pixelSize: Themes.targetSettings.fontSizes.button
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: id_button.danger
                ? id_button.down
                    ? Themes.targetSettings.colors.dangerBackgroundPressed
                    : id_button.hovered
                        ? Themes.targetSettings.colors.dangerBackgroundHover
                        : Themes.targetSettings.colors.dangerBackground
                : id_button.down
                    ? Themes.targetSettings.colors.buttonBackgroundPressed
                    : id_button.hovered
                        ? Themes.targetSettings.colors.buttonBackgroundHover
                        : Themes.targetSettings.colors.buttonBackground
            border.width: 1
            border.color: id_button.danger
                ? id_button.hovered
                    ? Themes.targetSettings.colors.dangerBorderHover
                    : Themes.targetSettings.colors.dangerBorder
                : id_button.hovered
                    ? Themes.targetSettings.colors.buttonBorderHover
                    : Themes.targetSettings.colors.buttonBorder

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }

        CustomTooltip {
            p_active: id_button.hovered
            p_delay: 300
            p_text: id_button.tooltipText
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    contentItem: ColumnLayout {
        id: id_content

        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Dashboard Settings")
            color: Themes.targetSettings.colors.titleText
            font.pixelSize: Themes.targetSettings.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        C_ActionButton {
            text: qsTr("Reload All Missing Metadata")
            tooltipText: qsTr("Reloads missing image assets and achievements metadata for every dashboard target")
            enabled: !ctxLymalink.steamHydrationBusy
            onClicked: {
                const targets = ctxLymalink.ReloadAllMissingMetadata()
                id_root.missingMetadataReloadQueued(targets)
                id_root.close()
            }
        }

        C_ActionButton {
            text: qsTr("Close")
            onClicked: id_root.close()
        }
    }
}
