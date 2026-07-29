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

        CustomButton {
            Layout.fillWidth: true
            implicitHeight: 40
            text: qsTr("Reload All Missing Metadata")
            enabled: !ctxLymalink.steamHydrationBusy
            onClicked: {
                const targets = ctxLymalink.ReloadAllMissingMetadata()
                id_root.missingMetadataReloadQueued(targets)
                id_root.close()
            }
        }
    }
}
