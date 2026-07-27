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
    property string p_targetType: "Emulator"
    property bool p_targetHidden: false
    property bool deleteConfirmVisible: false

    signal reloadAssetsRequested(int appId, string targetType)
    signal targetDataUpdated(int appId, string targetType)
    signal targetHiddenChanged(int appId, string targetType, bool hidden)
    signal targetDeleted(int appId, string targetType)

    // Internals _____________________________________________
    property bool targetHiddenState: p_targetHidden
    property string currentPrefixLocation: ""
    property string currentExecutableLocation: ""
    property string currentInstallationLocation: ""
    property bool steamUpdateLoading: false
    property bool manualScanLoading: false
    property bool manualScanCancelVisible: false
    property int manualScanTargetId: 0
    property bool passcodeUnlocked: false
    property bool awaitingUnlockAction: false
    property string unlockedSteamWebApiKey: ""
    property string steamUpdateStatusText: ""
    property bool steamUpdateStatusIsError: false

    readonly property bool steamConfigured: ctxSettings.steamId.trim().length > 0 && ctxSettings.steamWebApiKey !== ""
    readonly property bool backendServiceReady: typeof ctxBackendService !== "undefined" && ctxBackendService !== null
    readonly property bool backendServiceUsable: backendServiceReady && ctxBackendService.serviceAvailable && ctxBackendService.serviceActive
    readonly property var activeTargetIds: backendServiceReady ? ctxBackendService.activeTargetIds : []
    readonly property bool anyTargetIsActive: activeTargetIds.length > 0
    readonly property bool manualScanAvailable: p_targetType === "Emulator" && backendServiceUsable && !anyTargetIsActive && !manualScanLoading

    width: Math.min(340, parent ? parent.width - 48 : 340)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    onClosed: {
        id_root.cancelManualAchievementDataScan()
        deleteConfirmVisible = false
        id_deleteConfirmInput.text = ""
    }

    onOpened: {
        id_root.refreshTargetLocations()
        id_root.steamUpdateStatusText = ""
        id_root.steamUpdateStatusIsError = false
    }
    onP_targetHiddenChanged: targetHiddenState = p_targetHidden
    onP_appIdChanged: id_root.cancelManualAchievementDataScan()

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

        if (ctxLymalink.SetTargetHidden(id_root.p_appId, hidden, id_root.p_targetType)) {
            id_root.targetHiddenState = hidden
            id_root.targetHiddenChanged(id_root.p_appId, id_root.p_targetType, hidden)
        }
    }

    function refreshTargetLocations() {
        id_root.currentPrefixLocation = id_root.p_appId > 0 ? ctxLymalink.GetTargetPrefixLocation(id_root.p_appId) : ""
        id_root.currentExecutableLocation = id_root.p_appId > 0 ? ctxLymalink.GetTargetExecutableLocation(id_root.p_appId) : ""
        id_root.currentInstallationLocation = id_root.p_appId > 0 ? ctxLymalink.GetTargetInstallationLocation(id_root.p_appId) : ""
    }

    function reloadBackendTargets() {
        if (typeof ctxBackendService !== "undefined" && ctxBackendService !== null) {
            ctxBackendService.ReloadAllTargets()
        }
    }

    function manualScanTooltipText() {
        if (id_root.manualScanLoading) {
            return qsTr("Achievement data scan is already running")
        }
        if (!id_root.backendServiceUsable) {
            return qsTr("Background service must be running to rescan achievement data")
        }
        if (id_root.anyTargetIsActive) {
            return qsTr("Close all running games before rescanning achievement data")
        }
        return qsTr("Rescan for achievement and emulator data")
    }

    function beginManualAchievementDataScan() {
        if (id_root.p_appId <= 0 || !id_root.manualScanAvailable) {
            return
        }

        if (!ctxLymalink.ResetTargetAchievementDataLocation(id_root.p_appId)) {
            id_errorPopup.showError(qsTr("Couldn't Rescan Achievement Data"), ctxLymalink.GetLastOperationError())
            return
        }

        id_root.manualScanLoading = true
        id_root.manualScanCancelVisible = false
        id_root.manualScanTargetId = id_root.p_appId
        id_manualScanCancelButtonDelayTimer.restart()
        id_manualScanFallbackTimer.restart()
        id_root.targetDataUpdated(id_root.p_appId, id_root.p_targetType)
        ctxBackendService.StartManualAchievementDataScan(id_root.p_appId)
    }

    function cancelManualAchievementDataScan() {
        if (!id_root.manualScanLoading || id_root.manualScanTargetId <= 0) {
            return
        }

        const appId = id_root.manualScanTargetId
        if (id_root.backendServiceReady) {
            ctxBackendService.CancelManualAchievementDataScan(appId)
        }
        id_root.finishManualAchievementDataScan(appId)
    }

    function finishManualAchievementDataScan(appId) {
        if (!id_root.manualScanLoading || id_root.manualScanTargetId !== appId) {
            return
        }

        id_manualScanFallbackTimer.stop()
        id_manualScanCancelButtonDelayTimer.stop()
        id_root.manualScanLoading = false
        id_root.manualScanCancelVisible = false
        id_root.manualScanTargetId = 0
    }

    function setPrefixLocation(path) {
        if (id_root.p_appId <= 0 || path.length === 0) {
            return
        }

        if (path === id_root.currentPrefixLocation) {
            return
        }

        if (ctxLymalink.SetTargetPrefixLocation(id_root.p_appId, path)) {
            id_root.currentPrefixLocation = path
            id_root.targetDataUpdated(id_root.p_appId, id_root.p_targetType)
        }
    }

    function setExecutableLocation(path) {
        if (id_root.p_appId <= 0 || path.length === 0) {
            return
        }

        if (ctxLymalink.SetTargetExecutableLocation(id_root.p_appId, path)) {
            id_root.currentExecutableLocation = path
            id_root.reloadBackendTargets()
        } else {
            id_errorPopup.showError(qsTr("Couldn't Edit Executable Location"), ctxLymalink.GetLastOperationError())
        }
    }

    function setInstallationLocation(path) {
        if (id_root.p_appId <= 0) {
            return
        }

        if (path === id_root.currentInstallationLocation) {
            return
        }

        if (ctxLymalink.SetTargetInstallationLocation(id_root.p_appId, path)) {
            id_root.currentInstallationLocation = path
            id_root.targetDataUpdated(id_root.p_appId, id_root.p_targetType)
        } else {
            id_errorPopup.showError(qsTr("Couldn't Edit Installation Directory"), ctxLymalink.GetLastOperationError())
        }
    }

    function deleteTarget() {
        if (id_root.p_appId <= 0 || id_deleteConfirmInput.text !== "delete") {
            return
        }

        const appId = id_root.p_appId
        if (ctxLymalink.DeleteTarget(appId, id_root.p_targetType)) {
            id_root.targetDeleted(appId, id_root.p_targetType)
            id_root.close()
        }
    }

    function clearSteamPasscodeUnlock() {
        id_root.passcodeUnlocked = false
        id_root.awaitingUnlockAction = false
        id_root.unlockedSteamWebApiKey = ""
    }

    function beginSteamUpdate() {
        if (id_root.p_appId <= 0 || id_root.p_targetType !== "Steam" || id_root.steamUpdateLoading) {
            return
        }

        if (!id_root.steamConfigured) {
            id_root.steamUpdateStatusText = qsTr("Please configure both Steam ID and Web API key in Settings.")
            id_root.steamUpdateStatusIsError = true
            return
        }

        if (id_root.passcodeUnlocked) {
            id_steamUpdateTimer.restart()
        } else {
            id_passcodePopup.open()
        }
    }

    function updateSelectedSteamTarget() {
        if (!id_root.steamConfigured || id_root.unlockedSteamWebApiKey.length === 0) {
            id_root.steamUpdateStatusText = qsTr("Please unlock your Steam Web API key to update this target.")
            id_root.steamUpdateStatusIsError = true
            id_root.clearSteamPasscodeUnlock()
            return
        }

        id_root.steamUpdateLoading = true
        id_root.steamUpdateStatusText = qsTr("Updating from Steam...")
        id_root.steamUpdateStatusIsError = false

        const libraryPayload = ctxLymalink.FetchSteamOwnedGames(ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        if (!libraryPayload.success) {
            id_root.steamUpdateLoading = false
            id_root.steamUpdateStatusText = libraryPayload.error.length > 0
                ? libraryPayload.error
                : qsTr("Steam library could not be loaded.")
            id_root.steamUpdateStatusIsError = true
            id_root.clearSteamPasscodeUnlock()
            return
        }

        const games = libraryPayload.games ?? []
        const selectedGames = games.filter(function(game) {
            return game.appId === id_root.p_appId
        })

        if (selectedGames.length === 0) {
            id_root.steamUpdateLoading = false
            id_root.steamUpdateStatusText = qsTr("Selected target was not found in your Steam library.")
            id_root.steamUpdateStatusIsError = true
            id_root.clearSteamPasscodeUnlock()
            return
        }

        const updatePayload = ctxLymalink.UpdateSteamImports([selectedGames[0]], ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        id_root.steamUpdateLoading = false

        const updatedCount = updatePayload.updatedCount ?? 0
        const skippedCount = updatePayload.skippedCount ?? 0
        const errors = updatePayload.errors ?? []

        if (updatedCount > 0) {
            id_root.steamUpdateStatusText = qsTr("Updated Steam achievement data.")
            if (skippedCount > 0) {
                id_root.steamUpdateStatusText += " " + qsTr("%1 update(s) skipped or failed.").arg(skippedCount)
            }
            if (errors.length > 0) {
                id_root.steamUpdateStatusText += "\n" + errors.slice(0, 3).join("\n")
            }
            id_root.steamUpdateStatusIsError = errors.length > 0
            id_root.targetDataUpdated(id_root.p_appId, id_root.p_targetType)
            id_root.clearSteamPasscodeUnlock()
            return
        }

        id_root.steamUpdateStatusText = errors.length > 0
            ? errors.slice(0, 3).join("\n")
            : qsTr("Steam achievement data could not be updated.")
        id_root.steamUpdateStatusIsError = true
        id_root.clearSteamPasscodeUnlock()
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

    ConfirmationPopup {
        id: id_installationLocationPopup

        p_title: qsTr("Edit Installation Directory")
        p_description: qsTr("Adding an installation directory enables scanning for GOG Emulator.\nClear it to disable scanning for GOG Emulator.\n\nCurrent path:\n%1").arg(id_root.currentInstallationLocation.length > 0
            ? id_root.currentInstallationLocation
            : qsTr("Not set"))
        p_confirmText: qsTr("Apply")
        p_popupWidth: 520
        p_pathSelectionMode: true
        p_pathSelectionFolder: true
        p_pathSelectionRequired: false
        p_pathAllowClear: true
        p_pathDialogTitle: qsTr("Select Game Installation Directory")
        p_pathPlaceholderText: qsTr("Select Game Installation Directory")
        onConfirmed: function(path) {
            id_root.setInstallationLocation(path)
        }
    }

    ErrorPopup {
        id: id_errorPopup
    }

    ConfirmationPopup {
        id: id_passcodePopup

        p_title: qsTr("Unlock Steam Web API Key")
        p_description: qsTr("Enter passcode for saved Steam Web API key.")
        p_confirmText: qsTr("Continue")
        p_shortcutEnabled: true
        p_singleVerificationMode: true
        onClosed: {
            if (id_root.awaitingUnlockAction) {
                id_root.awaitingUnlockAction = false
                Qt.callLater(id_steamUpdateTimer.restart)
            } else {
                id_root.clearSteamPasscodeUnlock()
            }
        }
        onConfirmed: function(passcode) {
            ctxSettings.SetTempEncryptionKey(passcode)
            const unlockedKey = ctxSettings.GetSteamWebApiKeyPlain()
            ctxSettings.SetTempEncryptionKey("")

            if (unlockedKey.length === 0) {
                id_root.passcodeUnlocked = false
                id_root.unlockedSteamWebApiKey = ""
                id_root.steamUpdateStatusText = qsTr("Incorrect passcode. API key unlock failed.")
                id_root.steamUpdateStatusIsError = true
                return
            }

            id_root.passcodeUnlocked = true
            id_root.unlockedSteamWebApiKey = unlockedKey
            id_root.awaitingUnlockAction = true
            id_passcodePopup.close()
        }
    }

    Timer {
        id: id_steamUpdateTimer

        interval: 50
        repeat: false
        onTriggered: id_root.updateSelectedSteamTarget()
    }

    Timer {
        id: id_manualScanFallbackTimer

        interval: 32000
        repeat: false
        onTriggered: {
            if (id_root.manualScanTargetId > 0) {
                id_root.finishManualAchievementDataScan(id_root.manualScanTargetId)
            }
        }
    }

    // Delay timer before displaying manual scan button and busy indicator to prevent brief flashing
    Timer {
        id: id_manualScanCancelButtonDelayTimer

        interval: 1000
        repeat: false
        onTriggered: {
            id_root.manualScanCancelVisible = id_root.manualScanLoading
        }
    }

    Connections {
        target: typeof ctxBackendService !== "undefined" ? ctxBackendService : null

        function onSignalTargetDataChanged(appId) {
            id_root.finishManualAchievementDataScan(appId)
        }

        function onSignalManualAchievementDataScanFinished(appId, found, reason) {
            if (id_root.manualScanLoading && id_root.manualScanTargetId === appId) {
                id_root.targetDataUpdated(appId, id_root.p_targetType)
            }
            id_root.finishManualAchievementDataScan(appId)
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
            text: id_root.p_targetType === "Steam"
                ? qsTr("Manage imported Steam target data and visibility.")
                : qsTr("Manage local target data and visibility.")
            color: Themes.targetSettings.colors.bodyText
            font.pixelSize: Themes.targetSettings.fontSizes.body
            wrapMode: Text.WordWrap
        }

        C_ActionButton {
            id: id_reloadAchievementsButton

            text: qsTr("Reload Achievement Metadata")
            tooltipText: qsTr("Reloads image assets and achievements metadata")
            onClicked: {
                if (id_root.p_appId > 0) {
                    ctxLymalink.EnqueueSteamHydrationTask(id_root.p_appId, true, id_root.p_targetType)
                    id_root.reloadAssetsRequested(id_root.p_appId, id_root.p_targetType)
                    id_root.close()
                }
            }
        }

        C_ActionButton {
            id: id_rescanAchievementDataButton

            visible: id_root.p_targetType === "Emulator"
            text: qsTr("Rescan Achievement Data")
            tooltipText: id_root.manualScanTooltipText()
            enabled: id_root.manualScanAvailable
            opacity: enabled ? 1.0 : 0.55
            onClicked: id_root.beginManualAchievementDataScan()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: id_root.p_targetType === "Emulator" && id_root.manualScanLoading && id_root.manualScanCancelVisible

            CustomBusyIndicator {
                Layout.alignment: Qt.AlignVCenter
                p_indicatorSize: 22
                p_speed: 900
                p_running: id_root.manualScanLoading
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Scanning for achievement data...")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            id: id_manualScanCancelPanel

            Layout.fillWidth: true
            Layout.preferredHeight: id_root.manualScanCancelVisible ? implicitHeight : 0
            clip: true
            opacity: id_root.manualScanCancelVisible ? 1.0 : 0.0
            visible: id_root.manualScanCancelVisible || Layout.preferredHeight > 0
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

            C_ActionButton {
                id: id_manualScanCancelButton

                text: qsTr("Cancel")
                danger: true
                onClicked: id_root.cancelManualAchievementDataScan()
            }
        }

        C_ActionButton {
            id: id_updateFromSteamButton

            visible: id_root.p_targetType === "Steam"
            text: qsTr("Reload Steam Progress")
            tooltipText: qsTr("Reloads your progress on this target from Steam")
            enabled: !id_root.steamUpdateLoading
            opacity: enabled ? 1.0 : 0.55
            onClicked: id_root.beginSteamUpdate()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: id_root.p_targetType === "Steam" && (id_root.steamUpdateLoading || id_root.steamUpdateStatusText.length > 0)

            CustomBusyIndicator {
                Layout.alignment: Qt.AlignTop
                p_indicatorSize: 22
                p_speed: 900
                p_running: id_root.steamUpdateLoading
            }

            Label {
                Layout.fillWidth: true
                text: id_root.steamUpdateStatusText
                color: id_root.steamUpdateStatusIsError
                    ? Themes.targetSettings.colors.dangerText
                    : Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
                wrapMode: Text.WordWrap
            }
        }

        C_ActionButton {
            id: id_editExecutableLocationButton

            visible: id_root.p_targetType !== "Steam"
            text: qsTr("Edit Executable Location")
            tooltipText: qsTr("Select Game Executable")
            onClicked: id_executableLocationPopup.open()
        }

        C_ActionButton {
            id: id_editInstallationLocationButton

            visible: id_root.p_targetType === "Emulator"
            text: qsTr("Edit Installation Directory")
            tooltipText: qsTr("Select Game installation directory")
            onClicked: id_installationLocationPopup.open()
        }

        C_ActionButton {
            id: id_editPrefixLocationButton

            visible: !OS_WIN && id_root.p_targetType !== "Steam"
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

            CustomTextField {
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
