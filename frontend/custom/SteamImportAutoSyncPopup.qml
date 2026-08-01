/////////////////////////////////////////////////////////
// File: SteamImportAutoSyncPopup.qml
// Date: 2026-07-31
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Steam import automatic sync activation popup
/////////////////////////////////////////////////////////

import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    // Internals _____________________________________________
    property int selectedDurationDays: 0
    property string activationStatus: ""
    property bool activationCompleted: false

    signal activationCanceled()

    readonly property var durationOptions: [1, 3, 7, 14, 30, 60]
    readonly property bool canConfirm: id_passcodeInput.text.length >= 6
        && id_durationCombo.currentIndex >= 0

    parent: Overlay.overlay
    width: Math.min(420, parent ? parent.width - 48 : 420)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    function openActivation() {
        selectedDurationDays = 0
        activationStatus = ""
        activationCompleted = false
        id_durationCombo.currentIndex = -1
        id_passcodeInput.text = ""
        open()
        id_passcodeInput.forceActiveFocus()
    }

    function durationLabel(value) {
        return qsTr("%1 days").arg(value)
    }

    function confirmActivation() {
        if (!canConfirm) {
            return
        }

        ctxSettings.SetTempEncryptionKey(id_passcodeInput.text)
        const unlockedKey = ctxSettings.GetSteamWebApiKeyPlain()
        ctxSettings.SetTempEncryptionKey("")

        if (unlockedKey.length === 0) {
            activationStatus = qsTr("Incorrect passcode.")
            return
        }

        if (!ctxSettings.SaveValue(Settings.SteamImportAutoSyncEnabled, { "webApiKey": unlockedKey, "durationDays": selectedDurationDays, "intervalMinutes": ctxSettings.steamImportAutoSyncIntervalMinutes })) {
            activationStatus = qsTr("Automatic Steam progress sync could not be enabled.")
            return
        }

        activationCompleted = true
        close()
    }

    onClosed: {
        if (!activationCompleted) {
            activationCanceled()
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    Overlay.modal: Rectangle {
        color: Themes.steamImportAutoSyncPopup.colors.overlay
    }

    background: Rectangle {
        radius: 8
        color: Themes.steamImportAutoSyncPopup.colors.background
        border.width: 1
        border.color: Themes.steamImportAutoSyncPopup.colors.border
    }

    contentItem: ColumnLayout {
        id: id_content

        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Enable Automatic Steam Progress Sync")
            color: Themes.steamImportAutoSyncPopup.colors.titleText
            font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("To enable automatic sync for your imported Steam targets, enter the passcode for your Steam Web API key and choose how long sync stays enabled.")
            color: Themes.steamImportAutoSyncPopup.colors.bodyText
            font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.body
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            spacing: 10

            Image {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00007_ED.png"
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Syncing Steam progress...")
                    color: Themes.steamImportAutoSyncPopup.colors.titleText
                    font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.body
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("This appears in the sidebar while automatic sync is in progress.")
                    color: Themes.steamImportAutoSyncPopup.colors.bodyText
                    font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.sectionInfo
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Security note: Automatic sync temporarily stores an encrypted copy of your API key for the selected duration. It is used to unlock the API key during automatic sync. This is not secure for long-term storage if your device has been compromised or accessed without authorization.")
            color: Themes.steamImportAutoSyncPopup.colors.warningText
            font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.body
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        CustomTextField {
            id: id_passcodeInput

            Layout.fillWidth: true
            placeholderText: qsTr("API key passcode")
            echoMode: TextInput.Password
            onTextChanged: id_root.activationStatus = ""
        }

        CustomComboBox {
            id: id_durationCombo

            Layout.fillWidth: true
            model: id_root.durationOptions
            currentIndex: -1
            displayText: currentIndex >= 0 ? id_root.durationLabel(model[currentIndex]) : qsTr("Duration")
            p_textFromValue: function(value, index) { return id_root.durationLabel(value) }
            onActivated: function(index) {
                id_root.selectedDurationDays = model[index]
            }
        }

        Label {
            visible: id_root.activationStatus.length > 0
            Layout.fillWidth: true
            text: id_root.activationStatus
            color: Themes.steamImportAutoSyncPopup.colors.errorText
            font.pixelSize: Themes.steamImportAutoSyncPopup.fontSizes.sectionInfo
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CustomButton {
                Layout.fillWidth: true
                text: qsTr("Cancel")
                onClicked: id_root.close()
            }

            CustomButton {
                Layout.fillWidth: true
                enabled: id_root.canConfirm
                text: qsTr("Enable")
                onClicked: id_root.confirmActivation()
            }
        }
    }
}
