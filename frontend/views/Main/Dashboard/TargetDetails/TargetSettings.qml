/////////////////////////////////////////////////////////
// File: TargetSettings.qml
// Date: 2026-05-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Popup for target settings actions
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    property int p_appId: 0

    signal reloadAssetsRequested(int appId)

    width: 220
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 16
    closePolicy: Popup.CloseOnEscape
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.close()
    }

    background: Rectangle {
        radius: 8
        color: Themes.targetSettings.colors.background
        border.width: 1
        border.color: Themes.dashboardToolbar.colors.pillBorderHover
    }

    contentItem: ColumnLayout {
        id: id_content

        spacing: 10

        Button {
            id: id_reloadAchievementsButton

            Layout.fillWidth: true
            text: qsTr("Reload Achievement Data")
            onClicked: {
                if (id_root.p_appId > 0) {
                    ctxLymalink.EnqueueSteamHydrationTask(id_root.p_appId, true)
                    id_root.reloadAssetsRequested(id_root.p_appId)
                    id_root.close()
                }
            }

            CustomTooltip {
                active: id_reloadAchievementsButton.hovered
                delay: 300
                text: qsTr("Reloads image assets and achievements data")
            }
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Close")
            onClicked: id_root.close()
        }
    }
}
