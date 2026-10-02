/////////////////////////////////////////////////////////
// File: TargetLocationSettingsPopup.qml
// Date: 2026-10-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Staged target location settings editor
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property int p_appId: 0

    signal settingsApplied(int appId)

    // Internals _____________________________________________
    property string persistedExecutableLocation: ""
    property string persistedPrefixLocation: ""
    property string persistedInstallationLocation: ""
    property bool persistedManualLocation: false
    property string persistedEmulatorType: ""
    property string persistedAchievementLocation: ""
    property bool persistedAchievementLocationIsFolder: false

    property string draftExecutableLocation: ""
    property string draftPrefixLocation: ""
    property string draftInstallationLocation: ""
    property bool draftManualLocation: false
    property string draftEmulatorType: "CODEX"
    property bool draftAchievementLocationIsFolder: false
    property string draftAchievementFolder: ""
    property string draftAchievementFile: ""

    readonly property var emulatorTypeOptions: [
        { label: qsTr("CODEX / RUNE"), value: "CODEX", filePattern: "*.ini" },
        { label: qsTr("Goldberg"), value: "GOLDBERG", filePattern: "*.json" },
        { label: qsTr("Reloaded"), value: "RLD", filePattern: "*.ini" },
        { label: qsTr("SmartSteamEmu"), value: "SmartSteamEmu", filePattern: "*.bin" },
        { label: qsTr("Tenoke"), value: "Tenoke", filePattern: "*.ini" },
        { label: qsTr("NemirtingasGalaxyEmulator"), value: "GOG-N", filePattern: "*.json" }
    ]
    readonly property string draftAchievementLocation: draftAchievementLocationIsFolder
        ? draftAchievementFolder
        : draftAchievementFile
    readonly property bool canApply: id_root.requiredSettingsValid() && id_root.settingsChanged()

    ButtonGroup {
        id: id_achievementLocationTypeGroup
    }

    parent: Overlay.overlay
    width: Math.min(620, parent ? parent.width - 48 : 620)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    onClosed: id_root.restoreDraftSettings()

    Overlay.modal: Rectangle {
        color: Themes.targetSettings.colors.overlay
    }

    function openForTarget(appId) {
        id_root.p_appId = appId
        const settings = ctxLymalink.GetTargetLocationSettings(appId)
        if (Object.keys(settings).length === 0) {
            id_errorPopup.showError(qsTr("Couldn't Load Target Paths"), ctxLymalink.GetLastOperationError())
            return
        }

        id_root.persistedExecutableLocation = settings.executableLocation || ""
        id_root.persistedPrefixLocation = settings.prefixLocation || ""
        id_root.persistedInstallationLocation = settings.installationLocation || ""
        id_root.persistedManualLocation = Boolean(settings.customAchievementLocation)
        id_root.persistedEmulatorType = settings.emulatorType || ""
        id_root.persistedAchievementLocation = settings.achievementDataLocation || ""
        id_root.persistedAchievementLocationIsFolder = id_root.persistedManualLocation
            ? Boolean(settings.achievementLocationIsFolder)
            : false
        id_root.restoreDraftSettings()
        id_root.open()
    }

    function restoreDraftSettings() {
        id_root.draftExecutableLocation = id_root.persistedExecutableLocation
        id_root.draftPrefixLocation = id_root.persistedPrefixLocation
        id_root.draftInstallationLocation = id_root.persistedInstallationLocation
        id_root.draftManualLocation = id_root.persistedManualLocation
        id_root.draftEmulatorType = id_root.persistedEmulatorType.length > 0
            ? id_root.persistedEmulatorType
            : "CODEX"
        id_root.draftAchievementLocationIsFolder = id_root.persistedAchievementLocationIsFolder
        id_root.draftAchievementFolder = id_root.persistedAchievementLocationIsFolder
            ? id_root.persistedAchievementLocation
            : ""
        id_root.draftAchievementFile = id_root.persistedAchievementLocationIsFolder
            ? ""
            : id_root.persistedAchievementLocation
    }

    function requiredSettingsValid() {
        let settingsValid = id_root.draftExecutableLocation.trim().length > 0
        if (id_root.draftManualLocation) {
            const emulatorTypeSelected = id_root.draftEmulatorType.trim().length > 0
            const achievementLocationSelected = id_root.draftAchievementLocation.trim().length > 0
            settingsValid = settingsValid && emulatorTypeSelected && achievementLocationSelected
        } else if (!OS_WIN) {
            settingsValid = settingsValid && id_root.draftPrefixLocation.trim().length > 0
        }

        return settingsValid
    }

    function settingsChanged() {
        let settingsAreDifferent = id_root.draftExecutableLocation !== id_root.persistedExecutableLocation || id_root.draftManualLocation !== id_root.persistedManualLocation

        if (id_root.draftManualLocation) {
            settingsAreDifferent = settingsAreDifferent
                || id_root.draftEmulatorType !== id_root.persistedEmulatorType
                || id_root.draftAchievementLocation !== id_root.persistedAchievementLocation
                || id_root.draftAchievementLocationIsFolder !== id_root.persistedAchievementLocationIsFolder
        } else {
            settingsAreDifferent = settingsAreDifferent
                || id_root.draftPrefixLocation !== id_root.persistedPrefixLocation
                || id_root.draftInstallationLocation !== id_root.persistedInstallationLocation
        }

        return settingsAreDifferent
    }

    function emulatorTypeIndex(emulatorType) {
        let selectedIndex = 0
        for (let i = 0; i < id_root.emulatorTypeOptions.length; ++i) {
            if (id_root.emulatorTypeOptions[i].value === emulatorType) {
                selectedIndex = i
                break
            }
        }

        return selectedIndex
    }

    function selectedAchievementFilePattern() {
        const selectedIndex = id_root.emulatorTypeIndex(id_root.draftEmulatorType)
        const selectedOption = id_root.emulatorTypeOptions[selectedIndex]
        return selectedOption.filePattern
    }

    function selectedAchievementFileSuffix() {
        return id_root.selectedAchievementFilePattern().replace(/^\*/, "")
    }

    function achievementFileMatchesSelectedEmulator(filePath) {
        if (filePath.length === 0) {
            return true
        }

        return filePath.toLowerCase().endsWith(id_root.selectedAchievementFileSuffix().toLowerCase())
    }

    function selectEmulatorType(index) {
        id_root.draftEmulatorType = id_root.emulatorTypeOptions[index].value
        if (!id_root.achievementFileMatchesSelectedEmulator(id_root.draftAchievementFile)) {
            id_root.draftAchievementFile = ""
        }
    }

    function fileUrlToPath(fileUrl) {
        if (OS_WIN) {
            return decodeURIComponent(fileUrl.toString().replace(/^file:\/\/\//, ""))
        }

        return decodeURIComponent(fileUrl.toString().replace("file://", ""))
    }

    function applySettings() {
        if (!id_root.canApply) {
            return
        }

        const settingsSaved = ctxLymalink.SetTargetLocationSettings(
            id_root.p_appId,
            id_root.draftExecutableLocation,
            id_root.draftPrefixLocation,
            id_root.draftInstallationLocation,
            id_root.draftManualLocation,
            id_root.draftEmulatorType,
            id_root.draftAchievementLocation,
            id_root.draftAchievementLocationIsFolder
        )
        if (!settingsSaved) {
            id_errorPopup.showError(qsTr("Couldn't Save Target Paths"), ctxLymalink.GetLastOperationError())
            return
        }

        const appId = id_root.p_appId
        id_root.settingsApplied(appId)
        id_root.close()
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_ActionButton: CustomButton {
        id: id_button

        Layout.fillWidth: true
        implicitHeight: 40

        contentItem: Label {
            text: id_button.text
            color: id_button.enabled
                ? Themes.targetSettings.colors.buttonText
                : Themes.customButton.colors.textDisabled
            font.pixelSize: Themes.targetSettings.fontSizes.button
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: id_button.down
                ? Themes.targetSettings.colors.buttonBackgroundPressed
                : id_button.hovered && id_button.enabled
                    ? Themes.targetSettings.colors.buttonBackgroundHover
                    : Themes.targetSettings.colors.buttonBackground
            border.width: 1
            border.color: id_button.hovered && id_button.enabled
                ? Themes.targetSettings.colors.buttonBorderHover
                : Themes.targetSettings.colors.buttonBorder
            opacity: id_button.enabled ? 1.0 : 0.55
        }
    }

    component C_PathField: CustomTextField {
        id: id_pathField

        property string p_path: ""
        signal selected()

        Layout.fillWidth: true
        readOnly: true
        selectByMouse: false
        text: p_path
        color: Themes.targetSettings.colors.buttonText
        placeholderTextColor: Themes.targetSettings.colors.bodyText
        font.pixelSize: Themes.targetSettings.fontSizes.button

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: id_pathField.selected()
        }
    }

    component C_RadioButton: RadioButton {
        id: id_radioButton

        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            x: id_radioButton.leftPadding
            y: id_radioButton.topPadding + (id_radioButton.availableHeight - height) / 2
            radius: width / 2
            color: id_radioButton.checked
                ? Themes.customCheckBox.colors.backgroundChecked
                : Themes.customCheckBox.colors.background
            border.width: 1
            border.color: id_radioButton.visualFocus
                ? Themes.customCheckBox.colors.borderFocus
                : (id_radioButton.checked
                    ? Themes.customCheckBox.colors.borderChecked
                    : Themes.customCheckBox.colors.border)

            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: width / 2
                visible: id_radioButton.checked
                color: Themes.customCheckBox.colors.mark
            }
        }

        contentItem: Text {
            text: id_radioButton.text
            leftPadding: id_radioButton.indicator.width + 8
            color: Themes.customCheckBox.colors.text
            font.pixelSize: Themes.customCheckBox.fontSizes.text
            verticalAlignment: Text.AlignVCenter
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    FileDialog {
        id: id_executableDialog

        title: qsTr("Select Game Executable")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Executable files (*.exe)")]
        onAccepted: id_root.draftExecutableLocation = id_root.fileUrlToPath(selectedFile)
    }

    FolderDialog {
        id: id_prefixDialog

        title: qsTr("Select Prefix Location (drive_c or equivalent)")
        onAccepted: id_root.draftPrefixLocation = id_root.fileUrlToPath(selectedFolder)
    }

    FolderDialog {
        id: id_installationDialog

        title: qsTr("Select Game Installation Directory")
        onAccepted: id_root.draftInstallationLocation = id_root.fileUrlToPath(selectedFolder)
    }

    FolderDialog {
        id: id_achievementFolderDialog

        title: qsTr("Select Achievement Data Folder")
        onAccepted: {
            // Match persisted folder format before change detection.
            const folderPath = id_root.fileUrlToPath(selectedFolder).replace(/[\\/]+$/, "")
            id_root.draftAchievementFolder = folderPath + "/"
        }
    }

    FileDialog {
        id: id_achievementFileDialog

        title: qsTr("Select Achievement File")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Achievement files (%1)").arg(id_root.selectedAchievementFilePattern())]
        onAccepted: id_root.draftAchievementFile = id_root.fileUrlToPath(selectedFile)
    }

    ErrorPopup {
        id: id_errorPopup
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
            text: qsTr("Edit Target Paths")
            color: Themes.targetSettings.colors.titleText
            font.pixelSize: Themes.targetSettings.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        CustomTabSelector {
            Layout.fillWidth: true
            p_firstText: qsTr("Automatic detection")
            p_secondText: qsTr("Manual location")
            p_currentIndex: id_root.draftManualLocation ? 1 : 0
            onActivated: function(index) {
                id_root.draftManualLocation = index === 1
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Game Executable")
            color: Themes.targetSettings.colors.bodyText
            font.pixelSize: Themes.targetSettings.fontSizes.body
        }

        C_PathField {
            p_path: id_root.draftExecutableLocation
            placeholderText: qsTr("Select Game Executable")
            onSelected: id_executableDialog.open()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: !id_root.draftManualLocation

            Label {
                Layout.fillWidth: true
                visible: !OS_WIN
                text: qsTr("Prefix Location")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
            }

            C_PathField {
                visible: !OS_WIN
                p_path: id_root.draftPrefixLocation
                placeholderText: qsTr("Select Prefix Location (drive_c or equivalent)")
                onSelected: id_prefixDialog.open()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Game Installation Directory (Optional - Tenoke, NemirtingasGalaxyEmulator)")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                C_PathField {
                    p_path: id_root.draftInstallationLocation
                    placeholderText: qsTr("Select Game Installation Directory")
                    onSelected: id_installationDialog.open()
                }

                C_ActionButton {
                    Layout.fillWidth: false
                    Layout.preferredWidth: 82
                    text: qsTr("Clear")
                    enabled: id_root.draftInstallationLocation.length > 0
                    onClicked: id_root.draftInstallationLocation = ""
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: id_root.draftManualLocation

            Label {
                Layout.fillWidth: true
                text: qsTr("Emulator Type")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
            }

            CustomComboBox {
                Layout.fillWidth: true
                model: id_root.emulatorTypeOptions
                currentIndex: id_root.emulatorTypeIndex(id_root.draftEmulatorType)
                p_textFromValue: function(value) {
                    return value.label
                }
                onActivated: function(index) {
                    id_root.selectEmulatorType(index)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 24

                C_RadioButton {
                    text: qsTr("File")
                    checked: !id_root.draftAchievementLocationIsFolder
                    ButtonGroup.group: id_achievementLocationTypeGroup
                    onClicked: id_root.draftAchievementLocationIsFolder = false
                }

                C_RadioButton {
                    text: qsTr("Folder")
                    checked: id_root.draftAchievementLocationIsFolder
                    ButtonGroup.group: id_achievementLocationTypeGroup
                    onClicked: id_root.draftAchievementLocationIsFolder = true
                }
            }

            Label {
                Layout.fillWidth: true
                text: id_root.draftAchievementLocationIsFolder
                    ? qsTr("Folder where default achievement files appear")
                    : qsTr("Set current achievement file location")
                color: Themes.targetSettings.colors.bodyText
                font.pixelSize: Themes.targetSettings.fontSizes.body
            }

            C_PathField {
                p_path: id_root.draftAchievementLocation
                placeholderText: id_root.draftAchievementLocationIsFolder
                    ? qsTr("Select Achievement Data Folder")
                    : qsTr("Select Achievement %1 file").arg(id_root.selectedAchievementFileSuffix())
                onSelected: id_root.draftAchievementLocationIsFolder
                    ? id_achievementFolderDialog.open()
                    : id_achievementFileDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            C_ActionButton {
                text: qsTr("Cancel")
                onClicked: id_root.close()
            }

            C_ActionButton {
                text: qsTr("Apply")
                enabled: id_root.canApply
                onClicked: id_root.applySettings()
            }
        }
    }
}
