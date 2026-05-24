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

    // Public ________________________________________________
    property int p_appId: 0
    property bool p_targetHidden: false
    property bool deleteConfirmVisible: false

    signal reloadAssetsRequested(int appId)
    signal targetHiddenChanged(int appId, bool hidden)
    signal targetDeleted(int appId)

    // Internals _____________________________________________
    property bool targetHiddenState: p_targetHidden
    property string currentPrefixLocation: ""
    property string currentExecutableLocation: ""

    width: Math.min(340, parent ? parent.width - 48 : 340)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    onClosed: {
        deleteConfirmVisible = false
        id_deleteConfirmInput.text = ""
    }

    onOpened: id_root.refreshTargetLocations()
    onP_targetHiddenChanged: targetHiddenState = p_targetHidden

    onDeleteConfirmVisibleChanged: {
        if (!deleteConfirmVisible) {
            id_deleteConfirmInput.text = ""
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.close()
    }

    Overlay.modal: Rectangle {
        color: Themes.targetSettings.colors.overlay
    }

    function setTargetHidden(hidden) {
        if (id_root.p_appId <= 0) {
            return
        }

        if (ctxLymalink.SetTargetHidden(id_root.p_appId, hidden)) {
            id_root.targetHiddenState = hidden
            id_root.targetHiddenChanged(id_root.p_appId, hidden)
        }
    }

    function refreshTargetLocations() {
        id_root.currentPrefixLocation = id_root.p_appId > 0 ? ctxLymalink.GetTargetPrefixLocation(id_root.p_appId) : ""
        id_root.currentExecutableLocation = id_root.p_appId > 0 ? ctxLymalink.GetTargetExecutableLocation(id_root.p_appId) : ""
    }

    function reloadBackendTargets() {
        if (typeof ctxDBusService !== "undefined" && ctxDBusService !== null) {
            ctxDBusService.ReloadAllTargets()
        }
    }

    function setPrefixLocation(path) {
        if (id_root.p_appId <= 0 || path.length === 0) {
            return
        }

        if (ctxLymalink.SetTargetPrefixLocation(id_root.p_appId, path)) {
            id_root.currentPrefixLocation = path
            id_root.reloadBackendTargets()
        }
    }

    function setExecutableLocation(path) {
        if (id_root.p_appId <= 0 || path.length === 0) {
            return
        }

        if (ctxLymalink.SetTargetExecutableLocation(id_root.p_appId, path)) {
            id_root.currentExecutableLocation = path
            id_root.reloadBackendTargets()
        }
    }

    function deleteTarget() {
        if (id_root.p_appId <= 0 || id_deleteConfirmInput.text !== "delete") {
            return
        }

        const appId = id_root.p_appId
        if (ctxLymalink.DeleteTarget(appId)) {
            id_root.targetDeleted(appId)
            id_root.close()
        }
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////  

    component C_ActionButton: Button {
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
            active: id_button.hovered
            delay: 300
            text: id_button.tooltipText
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    ConfirmationPopup {
        id: id_prefixLocationPopup

        p_title: qsTr("Edit Prefix Location")
        p_description: qsTr("Current path:\n%1").arg(id_root.currentPrefixLocation.length > 0
            ? id_root.currentPrefixLocation
            : qsTr("Not set"))
        p_confirmText: qsTr("Apply")
        p_popupWidth: 520
        p_pathSelectionMode: true
        p_pathSelectionFolder: true
        p_pathDialogTitle: qsTr("Select Prefix Location (drive_c or equivalent)")
        p_pathPlaceholderText: qsTr("Select Prefix Location (drive_c or equivalent)")
        onConfirmed: function(path) {
            id_root.setPrefixLocation(path)
        }
    }

    ConfirmationPopup {
        id: id_executableLocationPopup

        p_title: qsTr("Edit Executable Location")
        p_description: qsTr("Current path:\n%1").arg(id_root.currentExecutableLocation.length > 0
            ? id_root.currentExecutableLocation
            : qsTr("Not set"))
        p_confirmText: qsTr("Apply")
        p_popupWidth: 520
        p_pathSelectionMode: true
        p_pathSelectionFolder: false
        p_pathDialogTitle: qsTr("Select Game Executable")
        p_pathPlaceholderText: qsTr("Select Game Executable")
        p_pathNameFilters: [qsTr("Executable files (*.exe)")]
        onConfirmed: function(path) {
            id_root.setExecutableLocation(path)
        }
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
            text: qsTr("Target Settings")
            color: Themes.targetSettings.colors.titleText
            font.pixelSize: Themes.targetSettings.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Manage local target data and visibility.")
            color: Themes.targetSettings.colors.bodyText
            font.pixelSize: Themes.targetSettings.fontSizes.body
            wrapMode: Text.WordWrap
        }

        C_ActionButton {
            id: id_reloadAchievementsButton

            text: qsTr("Reload Achievement Data")
            tooltipText: qsTr("Reloads image assets and achievement data")
            onClicked: {
                if (id_root.p_appId > 0) {
                    ctxLymalink.EnqueueSteamHydrationTask(id_root.p_appId, true)
                    id_root.reloadAssetsRequested(id_root.p_appId)
                    id_root.close()
                }
            }
        }

        C_ActionButton {
            id: id_editExecutableLocationButton

            text: qsTr("Edit Executable Location")
            tooltipText: qsTr("Select Game Executable")
            onClicked: id_executableLocationPopup.open()
        }

        C_ActionButton {
            id: id_editPrefixLocationButton

            text: qsTr("Edit Prefix Location")
            tooltipText: qsTr("Select Prefix Location (drive_c or equivalent)")
            onClicked: id_prefixLocationPopup.open()
        }
        
        C_ActionButton {
            id: id_hideButton

            visible: !id_root.targetHiddenState
            text: qsTr("Hide")
            tooltipText: qsTr("Hide this target from the dashboard unless the Hidden filter is active")
            onClicked: id_root.setTargetHidden(true)
        }

        C_ActionButton {
            id: id_unhideButton

            visible: id_root.targetHiddenState
            text: qsTr("Unhide")
            tooltipText: qsTr("Show this target in the dashboard again")
            onClicked: id_root.setTargetHidden(false)
        }

        C_ActionButton {
            id: id_deleteButton

            text: qsTr("Delete")
            danger: true
            tooltipText: qsTr("Delete this target from the database and remove its assets")
            onClicked: {
                id_root.deleteConfirmVisible = !id_root.deleteConfirmVisible
                if (id_root.deleteConfirmVisible) {
                    id_deleteConfirmInput.forceActiveFocus()
                }
            }
        }

        ColumnLayout {
            id: id_deleteConfirmPanel

            Layout.fillWidth: true
            Layout.preferredHeight: id_root.deleteConfirmVisible ? implicitHeight : 0
            clip: true
            opacity: id_root.deleteConfirmVisible ? 1.0 : 0.0
            visible: id_root.deleteConfirmVisible || Layout.preferredHeight > 0
            spacing: 8

            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: 160
                    easing.type: Easing.OutQuad
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 120
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Warning: Irreversible operation\n\nType \"delete\" to confirm.")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
                wrapMode: Text.WordWrap
            }

            TextField {
                id: id_deleteConfirmInput

                Layout.fillWidth: true
                enabled: id_root.deleteConfirmVisible
                placeholderText: qsTr("delete")
                echoMode: TextInput.Normal
                selectByMouse: true
                color: Themes.targetSettings.colors.buttonText
                placeholderTextColor: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.button

                background: Rectangle {
                    radius: 6
                    color: Themes.targetSettings.colors.buttonBackground
                    border.width: 1
                    border.color: id_deleteConfirmInput.activeFocus
                        ? Themes.targetSettings.colors.buttonBorderHover
                        : Themes.targetSettings.colors.buttonBorder
                }
            }

            C_ActionButton {
                id: id_deleteConfirmButton

                text: qsTr("Confirm")
                danger: true
                enabled: id_deleteConfirmInput.text === "delete"
                opacity: enabled ? 1.0 : 0.45
                onClicked: id_root.deleteTarget()
            }
        }

        C_ActionButton {
            id: id_closeButton

            text: qsTr("Close")
            onClicked: id_root.close()
        }
    }
}
