/////////////////////////////////////////////////////////
// File: Settings.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Settings page.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import QtCore as QtCore

Item {
    id: id_root

    // Internals _____________________________________________
    property string pendingSteamWebApiKey: ""
    property string achievementTransportStatus: ""
    property string pendingAchievementImportPath: ""
    property var achievementImportConflicts: []
    readonly property bool backendServiceReady: typeof ctxBackendService !== "undefined" && ctxBackendService !== null
    readonly property bool backendServiceStarting: backendServiceReady && ctxBackendService.serviceStarting
    readonly property bool backendServiceHealthy: backendServiceReady && ctxBackendService.serviceActive && ctxBackendService.serviceAvailable
    readonly property bool backendServiceEnabled: backendServiceReady && ctxBackendService.serviceEnabled
    readonly property int backendServiceState: backendServiceStarting ? 3 : (backendServiceHealthy ? (backendServiceEnabled ? 2 : 1) : 0)
    readonly property var activeTargetIds: backendServiceReady ? ctxBackendService.activeTargetIds : []
    readonly property bool hasActiveTarget: activeTargetIds.length > 0
    readonly property var overlayPositionValues: ["top-left", "top-center", "top-right", "bottom-right", "bottom-center", "bottom-left"]
    readonly property var overlayPositionLabels: [qsTr("Top-left"), qsTr("Top-center"), qsTr("Top-right"), qsTr("Bottom-right"), qsTr("Bottom-center"), qsTr("Bottom-left")]
    readonly property var overlayExitAnimationValues: ["slide-out", "fade-out"]
    readonly property var overlayExitAnimationLabels: [qsTr("Slide out"), qsTr("Fade out")]
    readonly property var steamImportAutoSyncIntervalOptions: [15, 30, 60, 180, 360, 720, 1440]

    signal achievementImportCompleted(var addedTargets)

    function saveSteamWebApiKey(apiKey, passcode) {
        ctxSettings.SetTempEncryptionKey(passcode)
        if (ctxSettings.SaveValue(Settings.SteamWebApiKey, apiKey)) {
            id_webApiKeyInput.completeApply(apiKey)
        }
    }

    function steamImportAutoSyncExpiryText() {
        if (!ctxSettings.steamImportAutoSyncEnabled || ctxSettings.steamImportAutoSyncExpiresAt <= 0) {
            return ""
        }

        const expires = new Date(ctxSettings.steamImportAutoSyncExpiresAt * 1000)
        return qsTr("Active until %1.").arg(Qt.formatDateTime(expires, Qt.DefaultLocaleShortDate))
    }

    function steamImportAutoSyncLastSyncText() {
        if (!ctxSettings.steamImportAutoSyncEnabled || ctxSettings.steamImportAutoSyncLastSyncedAt <= 0) {
            return ""
        }

        const syncedAt = new Date(ctxSettings.steamImportAutoSyncLastSyncedAt * 1000)
        return qsTr("Last sync %1.").arg(Qt.formatDateTime(syncedAt, Qt.DefaultLocaleShortDate))
    }

    function steamImportAutoSyncIntervalLabel(value) {
        if (value < 60) {
            return qsTr("%1 minutes").arg(value)
        }

        const hours = value / 60
        return hours === 1 ? qsTr("1 hour") : qsTr("%1 hours").arg(hours)
    }

    function fileUrlToPath(fileUrl) {
        if (OS_WIN) {
            return decodeURIComponent(fileUrl.toString().replace(/^file:\/\/\//, ""))
        }

        return decodeURIComponent(fileUrl.toString().replace("file://", ""))
    }

    function defaultAchievementExportName() {
        const today = new Date()
        const year = today.getFullYear()
        const month = String(today.getMonth() + 1).padStart(2, "0")
        const day = String(today.getDate()).padStart(2, "0")
        return "lymalink-achievements-export-%1-%2-%3.json".arg(year).arg(month).arg(day)
    }

    function setAchievementTransportStatus(message) {
        achievementTransportStatus = message
    }

    function exportAchievements(filePath) {
        const result = ctxDataTransporter.ExportAchievements(filePath)
        if (result.success) {
            setAchievementTransportStatus(qsTr("Exported %1 games and %2 achievements to %3")
                .arg(result.exportedGameCount)
                .arg(result.exportedAchievementCount)
                .arg(result.filePath))
        } else {
            setAchievementTransportStatus(qsTr("Export failed: %1").arg(result.error))
        }
    }

    function previewAchievementImport(filePath) {
        achievementImportConflicts = []
        pendingAchievementImportPath = ""

        const result = ctxDataTransporter.PreviewAchievementImport(filePath)
        if (!result.success) {
            setAchievementTransportStatus(qsTr("Import failed: %1").arg(result.error))
            return
        }

        pendingAchievementImportPath = filePath
        achievementImportConflicts = result.conflicts || []
        if (achievementImportConflicts.length === 0) {
            importAchievements(filePath, [])
            return
        }

        id_achievementImportConflictPopup.openConflicts(achievementImportConflicts)
    }

    function importAchievements(filePath, decisions) {
        const result = ctxDataTransporter.ImportAchievements(filePath, decisions)
        if (result.success) {
            pendingAchievementImportPath = ""
            achievementImportConflicts = []
            setAchievementTransportStatus(qsTr("Imported %1 targets and %2 achievements. Added %3, merged %4, replaced %5.")
                .arg(result.importedGameCount)
                .arg(result.importedAchievementCount)
                .arg(result.addedGameCount)
                .arg(result.mergedGameCount)
                .arg(result.replacedGameCount))
            id_root.achievementImportCompleted(result.addedTargets || [])
        } else {
            setAchievementTransportStatus(qsTr("Import failed: %1").arg(result.error))
        }
    }

    function overlayPositionIndex(value) {
        for (let i = 0; i < overlayPositionValues.length; ++i) {
            if (overlayPositionValues[i] === value) {
                return i
            }
        }
        return 3
    }

    function overlayExitAnimationIndex(value) {
        for (let i = 0; i < overlayExitAnimationValues.length; ++i) {
            if (overlayExitAnimationValues[i] === value) {
                return i
            }
        }
        return 0
    }

    function colorStyleLabel(value) {
        switch (value) {
            case -2: return qsTr("Grayscale")
            case -1: return qsTr("Disabled")
            case 0: return qsTr("Gold")
            case 1: return qsTr("Blue")
            case 2: return qsTr("Purple")
            case 3: return qsTr("Emerald")
            case 4: return qsTr("Ember")
            case 5: return qsTr("Frost")
        }
        return qsTr("Blue")
    }

    function saveProgressBarSelection(value) {
        ctxSettings.SaveValue(Settings.ProgressBarColorStyle, value)
    }

    function saveProgressFrameSelection(value) {
        ctxSettings.SaveValue(Settings.ProgressFrameColorStyle, value)
    }

    function saveTargetTypeBadgeSelection(value) {
        ctxSettings.SaveValue(Settings.TargetTypeBadgeColorStyle, value)
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Component - Section header component
    component C_SettingsSection: ColumnLayout {
        id: id_sectionRoot

        property string title: ""
        property string infoText: ""
        property bool fullRowMode: false
        default property alias content: id_contentHost.data

        Layout.fillWidth: true
        spacing: 14

        RowLayout {
            spacing: 10

            // Section title
            Label {
                text: id_sectionRoot.title.toUpperCase()
                color: Themes.settings.colors.sectionTitle
                font.pixelSize: Themes.settings.fontSizes.sectionTitle
                font.bold: true
                font.letterSpacing: 1.2
            }

            // Section title tail
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Themes.settings.colors.divider
            }
        }

        // Info text below section title
        Label {
            visible: id_sectionRoot.infoText !== ""
            text: id_sectionRoot.infoText
            Layout.fillWidth: true
            color: Themes.settings.colors.sectionInfo
            font.pixelSize: Themes.settings.fontSizes.sectionInfo
            wrapMode: Text.WordWrap
        }

        Item {
            id: id_contentHost
            
            visible: false
        }

        // Two columns on same row (2 x C_SettingRow)
        GridLayout {
            id: id_contentGrid

            visible: !id_sectionRoot.fullRowMode
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 32
            rowSpacing: 12
        }

        // Single column on same row (1 x C_SettingRow)
        ColumnLayout {
            id: id_rowSectionContent

            visible: id_sectionRoot.fullRowMode
            Layout.fillWidth: true
            spacing: 12
        }

        function applyContentLayout() {
            const target = id_sectionRoot.fullRowMode ? id_rowSectionContent : id_contentGrid
            const childrenToMove = id_contentHost.children.slice()
            for (let i = 0; i < childrenToMove.length; ++i) {
                childrenToMove[i].parent = target
            }
        }

        Component.onCompleted: Qt.callLater(applyContentLayout)
        onFullRowModeChanged: applyContentLayout()
    }

    // Component - Row for Single Settings Option
    component C_SettingRow: RowLayout {
        id: id_rowRoot

        property string label: ""
        property string tooltip: ""
        property int fixedWidthInt: 0

        Layout.preferredWidth: fixedWidthInt
        spacing: 12

        Label {
            visible: id_rowRoot.label !== ""
            text: id_rowRoot.label
            color: Themes.settings.colors.labelText
            font.pixelSize: Themes.settings.fontSizes.labelText
            
            Layout.fillWidth: visible && id_rowRoot.fixedWidthInt === 0
            Layout.minimumWidth: visible && fixedWidthInt !== 0 ? fixedWidthInt / 5 : 0
            
            elide: Text.ElideRight

            HoverHandler {
                id: id_labelHover
            }

            CustomTooltip {
                p_active: id_rowRoot.tooltip !== "" && id_labelHover.hovered
                p_delay: 600
                p_text: id_rowRoot.tooltip
            }
        }
    }

    // Component - Divider between setting rows
    component C_SettingDivider: Item {
        id: id_dividerRoot

        property int topPadding: 2
        property int bottomPadding: 2

        Layout.fillWidth: true
        Layout.columnSpan: 2
        Layout.preferredHeight: id_dividerRoot.topPadding + id_dividerLine.height + id_dividerRoot.bottomPadding

        Rectangle {
            id: id_dividerLine

            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            height: 1
            color: Themes.settings.colors.subDivider
        }
    }

    // Component - Info box
    component C_InfoBox: ColumnLayout {
        id: id_infoBoxRoot

        // Optional legend rows: list of { color, opacity, label } objects
        // e.g. [ { color: "#4caf50", opacity: Themes.serviceIndicator.opacity.solid, label: "Active" }, ... ]
        property var legend: []
        property alias text: id_infoLabel.text

        Layout.fillWidth: true
        spacing: 0

        // Box body
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: id_infoBoxInner.implicitHeight + 20
            radius: 6
            color: Themes.settings.colors.infoBoxBackground
            border.width: 1
            border.color: Themes.settings.colors.infoBoxBorder

            ColumnLayout {
                id: id_infoBoxInner

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: 14
                    rightMargin: 14
                    topMargin: 10
                }
                spacing: 12

                Label {
                    id: id_infoLabel
                    
                    Layout.fillWidth: true
                    color: Themes.settings.colors.sectionInfo
                    font.pixelSize: Themes.settings.fontSizes.sectionInfo
                    wrapMode: Text.WordWrap
                    lineHeight: 1.45
                }

                // Legend rows - rendered when legend is non-empty
                ColumnLayout {
                    visible: id_infoBoxRoot.legend.length > 0
                    Layout.fillWidth: true
                    spacing: 6

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Themes.settings.colors.infoBoxBorder
                    }

                    Repeater {
                        model: id_infoBoxRoot.legend

                        RowLayout {
                            width: parent.width
                            spacing: 10

                            // Status dot
                            Rectangle {
                                id: id_infoLegendDot

                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                radius: 6
                                color: modelData.color
                                opacity: modelData.hasOwnProperty("opacity") ? modelData.opacity : Themes.serviceIndicator.opacity.solid

                                SequentialAnimation on opacity {
                                    running: modelData.hasOwnProperty("breathing") && modelData.breathing
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        to: Themes.serviceIndicator.opacity.breathingLow
                                        duration: Themes.serviceIndicator.animation.breathingDuration
                                        easing.type: Easing.InOutSine
                                    }
                                    NumberAnimation {
                                        to: Themes.serviceIndicator.opacity.solid
                                        duration: Themes.serviceIndicator.animation.breathingDuration
                                        easing.type: Easing.InOutSine
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: Themes.settings.colors.sectionInfo
                                font.pixelSize: Themes.settings.fontSizes.sectionInfo
                                wrapMode: Text.WordWrap
                                lineHeight: 1.35
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 2
                    }
                }
            }
        }
    }

    // Component - Input field
    component C_ApplyInput: RowLayout {
        id: id_maskedInputRoot

        property alias inputText: id_input.text
        property bool enableMasking: false
        property bool initiallyMasked: false
        property string maskedText: "**********"
        property int fieldWidth: 280
        property int fieldHeight: 32
        property int flashDuration: 550
        property bool completeOnApply: true
        signal applyClicked(string text)

        spacing: 8
        onInitiallyMaskedChanged: {
            if (!initiallyMasked) {
                id_input.text = ""
                id_inputFrame.masked = false
            }
        }

        Rectangle {
            id: id_inputFrame

            property bool masked: id_maskedInputRoot.initiallyMasked
            property real flashOpacity: 0.0

            function beginEditing() {
                masked = false
                id_input.forceActiveFocus()
            }

            Layout.preferredWidth: id_maskedInputRoot.fieldWidth
            Layout.preferredHeight: id_maskedInputRoot.fieldHeight
            radius: 6
            color: "transparent"
            border.width: 1
            border.color: Themes.settings.colors.divider
            clip: true

            TextInput {
                id: id_input

                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: TextInput.AlignVCenter
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.labelText
                selectByMouse: true
                visible: !id_inputFrame.masked
                clip: true
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.labelText
                text: id_maskedInputRoot.maskedText
                visible: id_inputFrame.masked
            }

            TapHandler {
                enabled: id_inputFrame.masked
                onTapped: id_inputFrame.beginEditing()
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Themes.settings.colors.applyFlash
                opacity: id_inputFrame.flashOpacity
            }
        }

        function completeApply(submittedText) {
            if (id_maskedInputRoot.enableMasking && submittedText.length > 0) {
                id_input.text = ""
                id_inputFrame.masked = true
                id_flashAnim.restart()
            } else {
                id_inputFrame.masked = false
                id_flashAnim.restart()
            }
        }

        CustomButton {
            text: qsTr("Apply")
            onClicked: {
                const submittedText = id_input.text
                id_maskedInputRoot.applyClicked(submittedText)

                if (id_maskedInputRoot.completeOnApply) {
                    id_maskedInputRoot.completeApply(submittedText)
                }
            }
        }

        NumberAnimation {
            id: id_flashAnim

            target: id_inputFrame
            property: "flashOpacity"
            from: 0.45
            to: 0.0
            duration: id_maskedInputRoot.flashDuration
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Confirmation popup for API key passcode
    ConfirmationPopup {
        id: id_apiKeyConfirmationPopup

        p_title: qsTr("Protect API Key")
        p_description: qsTr("Set a passcode to encrypt this Steam Web API key before saving it.\n\nThe API key will be saved to: %1").arg(ctxSettings.GetConfigFilePath())
        p_confirmText: qsTr("Encrypt and Save")
        p_verificationMode: true
        onCanceled: id_root.pendingSteamWebApiKey = ""
        onClosed: id_root.pendingSteamWebApiKey = ""
        onConfirmed: (passcode) => {
            id_root.saveSteamWebApiKey(id_root.pendingSteamWebApiKey, passcode)
            id_root.pendingSteamWebApiKey = ""
        }
    }

    SteamImportAutoSyncPopup {
        id: id_steamImportAutoSyncActivationPopup

        onActivationCanceled: ctxSettings.SaveValue(Settings.SteamImportAutoSyncEnabled, false)
    }

    MarkdownDocumentPopup {
        id: id_markdownDocumentPopup
    }

    ConflictResolutionPopup {
        id: id_achievementImportConflictPopup

        p_title: qsTr("Import Existing Targets")
        p_description: qsTr("The imported data contains targets that already exist in your library.\nChoose how each target should be handled:\n\n" +
            "Merge: Adds missing achievements, updates progress only when imported progress is higher, and unlocks currently locked achievements. Existing unlock dates and game metadata remain unchanged.\n\n" +
            "Replace: Removes all existing achievements and replaces them with the imported data.")
        p_mergeText: qsTr("Merge")
        p_replaceText: qsTr("Replace")
        p_cancelText: qsTr("Cancel")
        p_confirmText: qsTr("Import")

        onCanceled: {
            ctxDataTransporter.ClearAchievementImportPreview()
            id_root.pendingAchievementImportPath = ""
            id_root.achievementImportConflicts = []
            id_root.setAchievementTransportStatus("")
            id_achievementImportConflictPopup.reset()
        }

        onConfirmed: function(decisions) {
            const path = id_root.pendingAchievementImportPath
            id_root.achievementImportConflicts = []
            id_achievementImportConflictPopup.reset()
            id_root.importAchievements(path, decisions)
        }
    }

    FileDialog {
        id: id_customNotificationSoundDialog

        title: qsTr("Select notification sound")
        fileMode: FileDialog.OpenFile
        nameFilters: OS_WIN
            ? [qsTr("Audio files (*.ogg *.wav *.mp3 *.flac)")]
            : [qsTr("Audio files (*.ogg *.wav)")]
        onAccepted: {
            const soundPath = id_root.fileUrlToPath(selectedFile)
            if (ctxSettings.SaveValue(Settings.CustomNotificationSoundPath, soundPath) && id_root.backendServiceReady) {
                ctxBackendService.ReloadConfig()
            }
        }
    }

    FileDialog {
        id: id_achievementExportDialog

        title: qsTr("Export emulator achievements")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        currentFile: QtCore.StandardPaths.writableLocation(QtCore.StandardPaths.DocumentsLocation) + "/" + id_root.defaultAchievementExportName()
        nameFilters: [qsTr("JSON files (*.json)")]
        onAccepted: id_root.exportAchievements(id_root.fileUrlToPath(selectedFile))
    }

    FileDialog {
        id: id_achievementImportDialog

        title: qsTr("Import emulator achievements")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JSON files (*.json)")]
        onAccepted: id_root.previewAchievementImport(id_root.fileUrlToPath(selectedFile))
    }

    // Fixed page header
    Item {
        id: id_fixedHeader

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: id_fixedHeaderLayout.implicitHeight

        ColumnLayout {
            id: id_fixedHeaderLayout

            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.right: parent.right
            anchors.rightMargin: Math.max(60, parent.width - 40 - 920)

            Item {
                Layout.preferredHeight: 48
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Settings")
                font.pixelSize: Themes.settings.fontSizes.titleText
                font.bold: true
                color: Themes.settings.colors.titleText
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 8
                color: Themes.settings.colors.divider
            }
        }
    }

    // Page
    ScrollView {
        id: id_settingsScrollView

        anchors.top: id_fixedHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: CustomScrollBar {
            id: id_verticalScrollBar

            policy: ScrollBar.AsNeeded
            p_rightMargin: 10
        }
        contentHeight: id_contentLayout.height
        clip: true

        Component.onCompleted: {
            id_verticalScrollBar.parent = id_settingsScrollView
            id_verticalScrollBar.anchors.top = id_settingsScrollView.top
            id_verticalScrollBar.anchors.bottom = id_settingsScrollView.bottom
            id_verticalScrollBar.anchors.right = id_settingsScrollView.right
        }

        Item {
            width: parent.width
            height: id_contentLayout.implicitHeight

            ColumnLayout {
                id: id_contentLayout

                // Pin to left with fixed margins, cap width on the right
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.right: parent.right
                anchors.rightMargin: Math.max(60, parent.width - 40 - 920)

                Item {
                    Layout.preferredHeight: 24
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 28

                    // Appearance
                    C_SettingsSection {
                        title: qsTr("Appearance")

                        C_SettingRow {
                            label: qsTr("Theme")
                            tooltip: qsTr("Controls the application's color theme")
                            CustomComboBox {
                                p_tooltipText: qsTr("Controls the application's color theme")
                                model: ["system", "dark", "light"]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.theme))
                                implicitWidth: 140
                                p_textFromValue: function(value, index) {
                                    switch (value) {
                                        case "system": return qsTr("System")
                                        case "dark": return qsTr("Dark")
                                        case "light": return qsTr("Light")
                                    }
                                    return qsTr("System")
                                }
                                onActivated: (index) => ctxSettings.SaveValue(Settings.Theme, model[index])
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Lymalink Logo")
                            tooltip: qsTr("Show or hide the Lymalink logo in the sidebar")
                            CustomSwitch {
                                checked: ctxSettings.showLymalinkLogo
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_logoSwitchHover }
                                CustomTooltip {
                                    p_active: id_logoSwitchHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show or hide the Lymalink logo in the sidebar")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowLymalinkLogo, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Language")
                            tooltip: qsTr("Sets the application's display language")
                            CustomComboBox {
                                p_tooltipText: qsTr("Sets the application's display language")
                                model: ["English"] //, "Finnish", "Svenska"]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.language))
                                implicitWidth: 140
                                onActivated: (index) => ctxSettings.SaveValue(Settings.Language, model[index])
                            }
                        }

                        C_SettingRow {}

                        C_SettingRow {
                            label: qsTr("Window size")
                            tooltip: qsTr("Reset the main window size to its default dimensions")

                            CustomButton {
                                Layout.preferredWidth: 140
                                text: qsTr("Reset to default")
                                p_tooltipText: qsTr("Reset the main window size to its default dimensions")
                                onClicked: {
                                    const win = id_root.Window.window
                                    if (!win) {
                                        return
                                    }
                                    win.showNormal()
                                    win.width = ctxSettings.windowSizeXDefault
                                    win.height = ctxSettings.windowSizeYDefault
                                }
                            }
                        }
                    }

                    // Interface
                    C_SettingsSection {
                        title: qsTr("Interface")

                        C_SettingRow {
                            label: qsTr("Close to tray")
                            tooltip: qsTr("When closing the window, minimize the application to the system tray instead of exiting")
                            ColumnLayout {
                                spacing: 4

                                CustomSwitch {
                                    enabled: ctxSysTray.available
                                    checked: ctxSettings.closeToTray && ctxSysTray.available
                                    text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                    HoverHandler { id: id_trayHover }
                                    CustomTooltip {
                                        p_active: id_trayHover.hovered
                                        p_delay: 600
                                        p_text: ctxSysTray.available
                                            ? qsTr("When closing the window, minimize the application to the system tray instead of exiting")
                                            : qsTr("System tray is not available in this desktop session")
                                    }
                                    onToggled: ctxSettings.SaveValue(Settings.CloseToTray, checked)
                                }

                                Label {
                                    visible: !ctxSysTray.available
                                    Layout.maximumWidth: 260
                                    text: qsTr("System tray is unavailable. On GNOME, enable an AppIndicator/KStatusNotifier extension first.")
                                    color: Themes.settings.colors.sectionInfo
                                    font.pixelSize: Themes.settings.fontSizes.sectionInfo
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse button")
                            tooltip: qsTr("Show a button for collapsing the sidebar")
                            CustomSwitch {
                                checked: ctxSettings.showCollapseButton
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_collapseButtonHover }
                                CustomTooltip {
                                    p_active: id_collapseButtonHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show a button for collapsing the sidebar")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowCollapseButton, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Close to tray notification")
                            tooltip: qsTr("Show system notification while closing to tray")
                            CustomSwitch {
                                enabled: ctxSettings.closeToTray && ctxSysTray.available
                                checked: ctxSettings.closeToTrayToast && enabled
                                text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_trayToastHover }
                                CustomTooltip {
                                    p_active: id_trayToastHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show system notification while closing to tray")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.CloseToTrayToast, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse border button")
                            tooltip: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                            CustomSwitch {
                                checked: ctxSettings.enableCollapseBorderButton
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_collapseBorderHover }
                                CustomTooltip {
                                    p_active: id_collapseBorderHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableCollapseBorderButton, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Tooltips")
                            tooltip: qsTr("Show tooltips")
                            CustomSwitch {
                                checked: ctxSettings.showTooltips
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_tooltipsHover }
                                CustomTooltip {
                                    p_active: id_tooltipsHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show tooltips")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowTooltips, checked)
                            }
                        }
                    }

                    // Display
                    C_SettingsSection {
                        title: qsTr("Display")

                        C_SettingRow {
                            label: qsTr("Color theme")
                            tooltip: qsTr("Select color theme for the application")
                            CustomComboBox {
                                p_tooltipText: qsTr("Select color theme for the application")
                                model: [0, 1, 2, 3, 4, 5]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.globalColorStyle))
                                implicitWidth: 150
                                p_textFromValue: function(value, index) {
                                    switch (value) {
                                        case 0: return qsTr("Gold")
                                        case 1: return qsTr("Blue")
                                        case 2: return qsTr("Purple")
                                        case 3: return qsTr("Emerald")
                                        case 4: return qsTr("Ember")
                                        case 5: return qsTr("Frost")
                                    }
                                    return qsTr("Gold")
                                }
                                onActivated: (index) => ctxSettings.SaveValue(Settings.GlobalColorStyle, model[index])
                            }
                        }

                        C_SettingRow {}

                        C_SettingRow {
                            label: qsTr("Dynamic achievement rows")
                            tooltip: qsTr("Achievement rows resize automatically to use available window space")
                            CustomSwitch {
                                checked: ctxSettings.enableDynamicAchievementRows
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_dynamicAchievementRows }
                                CustomTooltip {
                                    p_active: id_dynamicAchievementRows.hovered
                                    p_delay: 600
                                    p_text: qsTr("Achievement rows resize automatically to use available window space")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableDynamicAchievementRows, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Total achievements badge")
                            tooltip: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                            CustomSwitch {
                                checked: ctxSettings.showTotalAchievementsBadge
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_achieveBadgeHover }
                                CustomTooltip {
                                    p_active: id_achieveBadgeHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowTotalAchievementsBadge, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Installation status badge")
                            tooltip: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                            CustomSwitch {
                                checked: ctxSettings.showInstallationStatusBadge
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_installIconHover }
                                CustomTooltip {
                                    p_active: id_installIconHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowInstallationStatusBadge, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Target type badge")
                            tooltip: qsTr("Show a badge on cards indicating whether the target is Custom, Steam, or Emulator")
                            CustomComboBox {
                                p_tooltipText: qsTr("Show a badge on cards indicating whether the target is Custom, Steam, or Emulator")
                                model: [-1, 0, 1, 2, 3, 4, 5]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.targetTypeBadgeColorStyle))
                                implicitWidth: 150
                                p_textFromValue: function(value, index) { return id_root.colorStyleLabel(value) }
                                onActivated: (index) => id_root.saveTargetTypeBadgeSelection(model[index])
                            }
                        }

                        C_SettingDivider {}

                        C_SettingRow {
                            label: qsTr("Progress bar")
                            tooltip: qsTr("Select color theme for the card progress bar, or disable it")
                            CustomComboBox {
                                p_tooltipText: qsTr("Select color theme for the card progress bar, or disable it")
                                model: [-1, 0, 1, 2, 3, 4, 5]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.progressBarColorStyle))
                                implicitWidth: 150
                                p_textFromValue: function(value, index) { return id_root.colorStyleLabel(value) }
                                onActivated: (index) => id_root.saveProgressBarSelection(model[index])
                            }
                        }

                        C_SettingRow {}

                        C_SettingRow {
                            label: qsTr("Progress frame")
                            tooltip: qsTr("Select color theme for the card progress frame, grayscale mode, or disable it")
                            CustomComboBox {
                                p_tooltipText: qsTr("Select color theme for the card progress frame, grayscale mode, or disable it")
                                model: [-1, -2, 0, 1, 2, 3, 4, 5]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.progressFrameColorStyle))
                                implicitWidth: 150
                                p_textFromValue: function(value, index) { return id_root.colorStyleLabel(value) }
                                onActivated: (index) => id_root.saveProgressFrameSelection(model[index])
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame completion animation")
                            tooltip: qsTr("Play a subtle breath animation on completed card progress frame - not available in grayscale mode")
                            CustomSwitch {
                                enabled: ctxSettings.progressFrameColorStyle >= 0
                                checked: ctxSettings.enableProgressFrameCompletionAnimation && enabled
                                text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_progressAnimHover }
                                CustomTooltip {
                                    p_active: id_progressAnimHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Play a subtle breath animation on completed card progress frame - not available in grayscale mode")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableProgressFrameCompletionAnimation, checked)
                            }
                        }                    
                    }

                    // Backend Service
                    C_SettingsSection {
                        title: qsTr("Background Service")

                        C_InfoBox {
                            Layout.columnSpan: 2 // span both grid columns
                            Layout.fillWidth: true

                            text: qsTr(
                                "Background Service controls achievement tracking in the background. When 'Track in Background' option is enabled, " +
                                "a system service keeps tracking and notifications active even after Lymalink is closed. " +
                                "When disabled, service is running and tracking achievements only while Lymalink is open."
                            )

                            legend: [
                                { color: Themes.serviceIndicator.colors.running, opacity: Themes.serviceIndicator.opacity.solid, label: qsTr("Background service is running independently. Achievement tracking and notifications work even when Lymalink is closed.") },
                                { color: Themes.serviceIndicator.colors.running, opacity: Themes.serviceIndicator.opacity.solid, breathing: true, label: qsTr("Background service is active and responding. Achievement tracking and notifications work only when Lymalink is open.") },
                                { color: Themes.serviceIndicator.colors.starting, opacity: Themes.serviceIndicator.opacity.solid, label: qsTr("Background service is starting. Tracking will resume when the service responds.") },
                                { color: Themes.serviceIndicator.colors.error, opacity: Themes.serviceIndicator.opacity.solid, label: qsTr("Error: Background service did not respond. Achievement tracking is unavailable.") }
                            ]
                        }

                        C_SettingRow {
                            label: qsTr("Track in Background")
                            tooltip: qsTr("Keep tracking active even when the application is closed")

                            CustomSwitch {
                                id: id_backendServiceSwitch

                                enabled: id_root.backendServiceReady
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_backendServiceHover }
                                CustomTooltip {
                                    p_active: id_backendServiceHover.hovered; p_delay: 600
                                    p_text: qsTr("Keep tracking active even when the application is closed")
                                }
                                Binding on checked {
                                    value: id_root.backendServiceEnabled
                                }
                                onToggled: {
                                    const intendedEnabled = checked
                                    if (id_root.backendServiceReady) {
                                        ctxBackendService.SetServiceEnabled(intendedEnabled)
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Service status")
                            tooltip: qsTr("Current system service state")

                            RowLayout {
                                spacing: 10

                                Rectangle {
                                    id: id_serviceStatusDot

                                    readonly property bool breathing: id_root.backendServiceState === 1

                                    Layout.preferredWidth: 12
                                    Layout.preferredHeight: 12
                                    radius: 6
                                    color: id_root.backendServiceState === 3
                                        ? Themes.serviceIndicator.colors.starting
                                        : (id_root.backendServiceState === 1 || id_root.backendServiceState === 2
                                            ? Themes.serviceIndicator.colors.running
                                            : Themes.serviceIndicator.colors.error)
                                    onBreathingChanged: if (!breathing) opacity = Themes.serviceIndicator.opacity.solid

                                    SequentialAnimation on opacity {
                                        running: id_serviceStatusDot.breathing
                                        loops: Animation.Infinite
                                        NumberAnimation {
                                            to: Themes.serviceIndicator.opacity.breathingLow
                                            duration: Themes.serviceIndicator.animation.breathingDuration
                                            easing.type: Easing.InOutSine
                                        }
                                        NumberAnimation {
                                            to: Themes.serviceIndicator.opacity.solid
                                            duration: Themes.serviceIndicator.animation.breathingDuration
                                            easing.type: Easing.InOutSine
                                        }
                                    }
                                }

                                Label {
                                    Layout.preferredWidth: 60
                                    text: id_root.backendServiceState === 3
                                        ? qsTr("Starting")
                                        : (id_root.backendServiceState === 1 || id_root.backendServiceState === 2
                                            ? (id_root.backendServiceState === 2 ? qsTr("Running") : qsTr("Running"))
                                            : qsTr("Error"))
                                    color: Themes.settings.colors.labelText
                                    font.pixelSize: Themes.settings.fontSizes.labelText
                                }

                                CustomButton {
                                    text: id_root.backendServiceStarting ? qsTr("Starting...") : qsTr("Restart")
                                    p_tooltipText: id_root.backendServiceStarting
                                        ? qsTr("Background service is already starting")
                                        : qsTr("Restart the background service")
                                    enabled: id_root.backendServiceReady && !id_root.backendServiceStarting
                                    onClicked: ctxBackendService.RestartService()
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Overlay position")
                            tooltip: qsTr("Select where achievement overlay notifications appear")

                            CustomComboBox {
                                id: id_overlayPositionCombo
                                p_tooltipText: qsTr("Select where achievement overlay notifications appear")

                                model: id_root.overlayPositionLabels
                                currentIndex: id_root.overlayPositionIndex(ctxSettings.overlayNotificationPosition)
                                implicitWidth: 140

                                onActivated: (index) => {
                                    const position = id_root.overlayPositionValues[index]
                                    if (ctxSettings.SaveValue(Settings.OverlayNotificationPosition, position) && id_root.backendServiceReady) {
                                        ctxBackendService.ReloadConfig()
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Exit animation")
                            tooltip: qsTr("Select how achievement overlay notifications disappear")

                            CustomComboBox {
                                id: id_overlayExitAnimationCombo
                                p_tooltipText: qsTr("Select how achievement overlay notifications disappear")

                                model: id_root.overlayExitAnimationLabels
                                currentIndex: id_root.overlayExitAnimationIndex(ctxSettings.overlayNotificationExitAnimation)
                                implicitWidth: 140

                                onActivated: (index) => {
                                    const animation = id_root.overlayExitAnimationValues[index]
                                    if (ctxSettings.SaveValue(Settings.OverlayNotificationExitAnimation, animation) && id_root.backendServiceReady) {
                                        ctxBackendService.ReloadConfig()
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Overlay notification")
                            tooltip: id_root.hasActiveTarget
                                ? qsTr("Send a test overlay notification to a running tracked target")
                                : qsTr("Start at least one tracked target before sending a test overlay notification")

                            RowLayout {
                                spacing: 22

                                // Overlay Notification Info box
                                Rectangle {
                                    id: id_overlayTestInfoIcon

                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: "transparent"
                                    border.width: 1
                                    border.color: Themes.settings.colors.labelText
                                    Layout.alignment: Qt.AlignVCenter

                                    Text {
                                        anchors.centerIn: parent
                                        text: "i"
                                        font.pixelSize: 11
                                        font.italic: true
                                        font.bold: true
                                        color: Themes.settings.colors.labelText
                                    }

                                    HoverHandler {
                                        id: id_overlayTestInfoHover
                                    }

                                    CustomTooltip {
                                        p_active: id_overlayTestInfoHover.hovered
                                        p_alwaysVisible: true
                                        p_delay: 200
                                        p_maxLineCount: 6
                                        p_text: qsTr(
                                            qsTr("Sends a test overlay notification to verify that overlay is working correctly.\n") +
                                            qsTr("1. Make sure game you are tracking is running.\n") +
                                            qsTr("2. Press 'Send test'.\n") +
                                            qsTr("3. Notification appears inside game window and a sound will be played.")
                                        )
                                    }
                                }

                                // Send overlay notification test button
                                CustomButton {
                                    Layout.preferredWidth: 140
                                    text: qsTr("Send test")
                                    p_tooltipText: id_root.hasActiveTarget
                                        ? qsTr("Send a test overlay notification to a running tracked target")
                                        : qsTr("Start at least one tracked target before sending a test overlay notification")
                                    enabled: id_root.backendServiceReady && id_root.backendServiceHealthy && id_root.hasActiveTarget
                                    onClicked: ctxBackendService.TestToast()
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Startup Notification")
                            tooltip: qsTr("Show an overlay notification shortly after a tracked game starts")

                            CustomSwitch {
                                checked: ctxSettings.startupNotification
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_startupNotificationHover }
                                CustomTooltip {
                                    p_active: id_startupNotificationHover.hovered
                                    p_delay: 600
                                    p_text: qsTr("Show an overlay notification shortly after a tracked game starts")
                                }
                                onToggled: {
                                    if (ctxSettings.SaveValue(Settings.StartupNotification, checked) && id_root.backendServiceReady) {
                                        ctxBackendService.ReloadConfig()
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Notification sound")
                            tooltip: qsTr("Select the achievement notification sound")

                            RowLayout {
                                spacing: 8

                                CustomButton {
                                    text: qsTr("Test")
                                    p_tooltipText: qsTr("Play the currently selected notification sound")
                                    enabled: id_root.backendServiceReady && id_root.backendServiceHealthy
                                        && (id_notificationSoundCombo.count > 0 || ctxSettings.customNotificationSound)
                                    onClicked: ctxBackendService.TestSound()
                                }

                                CustomComboBox {
                                    id: id_notificationSoundCombo
                                    p_tooltipText: qsTr("Select the achievement notification sound")

                                    model: ctxSettings.notificationSounds
                                    enabled: count > 0 && !ctxSettings.customNotificationSound
                                    currentIndex: Math.max(0, model.indexOf(ctxSettings.notificationSound))
                                    implicitWidth: 140
                                    p_visibleRows: 7
                                    p_rowHeight: 32
                                    p_textFromValue: function(value, index) { return qsTr("Sound %1").arg(index + 1) }
                                    displayText: enabled ? qsTr("Sound %1").arg(currentIndex + 1) : qsTr("No sounds")
                                    onActivated: (index) => {
                                        if (ctxSettings.SaveValue(Settings.NotificationSound, model[index]) && id_root.backendServiceReady) {
                                            ctxBackendService.ReloadConfig()
                                        }
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            label: OS_WIN
                                ? qsTr("Custom notification sound (.ogg, .wav, .mp3, .flac)")
                                : qsTr("Custom notification sound (.ogg, .wav)")
                            tooltip: OS_WIN
                                ? qsTr("Use a custom .ogg, .wav, .mp3, or .flac sound file instead of the bundled notification sound")
                                : qsTr("Use a custom .ogg or .wav sound file instead of the bundled notification sound")

                            CustomSwitch {
                                checked: ctxSettings.customNotificationSound
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                onToggled: {
                                    if (ctxSettings.SaveValue(Settings.CustomNotificationSound, checked) && id_root.backendServiceReady) {
                                        ctxBackendService.ReloadConfig()
                                    }
                                }
                            }
                        }

                        C_SettingRow {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true

                            RowLayout {
                                Layout.fillWidth: true
                                enabled: ctxSettings.customNotificationSound
                                spacing: 8

                                CustomTextField {
                                    id: id_customNotificationSoundPathField

                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 120
                                    readOnly: true
                                    selectByMouse: true
                                    text: ctxSettings.customNotificationSoundPath
                                    placeholderText: OS_WIN
                                        ? qsTr("Select .ogg, .wav, .mp3, or .flac file")
                                        : qsTr("Select .ogg or .wav file")
                                }

                                CustomButton {
                                    text: qsTr("Browse")
                                    p_tooltipText: OS_WIN
                                        ? qsTr("Choose a custom .ogg, .wav, .mp3, or .flac notification sound file")
                                        : qsTr("Choose a custom .ogg or .wav notification sound file")
                                    onClicked: id_customNotificationSoundDialog.open()
                                }
                            }
                        }
                    }

                    // Steam API
                    C_SettingsSection {
                        title: qsTr("Steam Web API")

                        C_InfoBox {
                            Layout.columnSpan: 2 // span both grid columns
                            Layout.fillWidth: true

                            text: qsTr(
                                "The Steam Web API can be used to import your Steam achievement progress into Lymalink.\n" +
                                "When saving an API key, you will be asked to create a passcode with at least 6 characters. " +
                                "The API key will be encrypted with that passcode before being stored locally."
                            )
                        }

                        C_SettingRow {
                            Layout.columnSpan: 2
                            label: qsTr("Steam ID")
                            tooltip: qsTr("Steam ID is a long numeric account identifier - You can find it on your Steam Account page")
                            fixedWidthInt: 500

                            C_ApplyInput {
                                inputText: ctxSettings.steamId
                                onApplyClicked: (text) => ctxSettings.SaveValue(Settings.SteamId, text)
                            }
                        }

                        C_SettingRow {
                            Layout.columnSpan: 2
                            label: qsTr("Web API Key")
                            tooltip: qsTr("Get your Steam Web API key from https://steamcommunity.com/dev/apikey")
                            fixedWidthInt: 500

                            C_ApplyInput {
                                id: id_webApiKeyInput

                                enableMasking: true
                                completeOnApply: false
                                initiallyMasked: ctxSettings.steamWebApiKey !== ""
                                onApplyClicked: (text) => {
                                    if (text === "") {
                                        ctxSettings.SaveValue(Settings.SteamWebApiKey, "reset")
                                        id_webApiKeyInput.completeApply(text)
                                        return
                                    }

                                    id_root.pendingSteamWebApiKey = text
                                    id_apiKeyConfirmationPopup.open()
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Automatic Steam progress sync")
                            tooltip: qsTr("Periodically sync imported Steam progress from Steam")

                            ColumnLayout {
                                spacing: 4

                                CustomSwitch {
                                    id: id_steamImportAutoSyncSwitch

                                    enabled: ctxSettings.steamId.trim().length > 0 && ctxSettings.steamWebApiKey !== ""
                                    text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                    Binding on checked {
                                        value: ctxSettings.steamImportAutoSyncEnabled
                                    }
                                    HoverHandler { id: id_steamImportAutoSyncHover }
                                    CustomTooltip {
                                        p_active: id_steamImportAutoSyncHover.hovered
                                        p_delay: 600
                                        p_text: qsTr("Periodically sync imported Steam progress from Steam")
                                    }
                                    onToggled: {
                                        if (checked) {
                                            id_steamImportAutoSyncActivationPopup.openActivation()
                                        } else {
                                            ctxSettings.SaveValue(Settings.SteamImportAutoSyncEnabled, false)
                                        }
                                    }
                                }

                                Label {
                                    visible: ctxSettings.steamImportAutoSyncEnabled && id_root.steamImportAutoSyncExpiryText().length > 0
                                    text: id_root.steamImportAutoSyncExpiryText()
                                    color: Themes.settings.colors.sectionInfo
                                    font.pixelSize: Themes.settings.fontSizes.sectionInfo
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Sync interval")
                            tooltip: qsTr("How often automatic Steam progress sync checks for changes")

                            ColumnLayout {
                                spacing: 4

                                CustomComboBox {
                                    enabled: ctxSettings.steamImportAutoSyncEnabled
                                    p_tooltipText: qsTr("How often automatic Steam progress sync checks for changes")
                                    model: id_root.steamImportAutoSyncIntervalOptions
                                    currentIndex: Math.max(0, model.indexOf(ctxSettings.steamImportAutoSyncIntervalMinutes))
                                    implicitWidth: 150
                                    p_textFromValue: function(value, index) {
                                        return id_root.steamImportAutoSyncIntervalLabel(value)
                                    }
                                    onActivated: function(index) {
                                        ctxSettings.SaveValue(Settings.SteamImportAutoSyncIntervalMinutes, model[index])
                                    }
                                }

                                Label {
                                    visible: ctxSettings.steamImportAutoSyncEnabled && id_root.steamImportAutoSyncLastSyncText().length > 0
                                    text: id_root.steamImportAutoSyncLastSyncText()
                                    color: Themes.settings.colors.sectionInfo
                                    font.pixelSize: Themes.settings.fontSizes.sectionInfo
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    // Import / Export
                    C_SettingsSection {
                        fullRowMode: true
                        title: qsTr("Import / Export")
                        infoText: "Export or import Emulator achievement data.\nNote: Hidden exported targets will remain hidden after being imported."

                        C_SettingRow {
                            fixedWidthInt: 500

                            RowLayout {
                                spacing: 8

                                CustomButton {
                                    text: qsTr("Import")
                                    p_tooltipText: qsTr("Import Emulator achievement data from a JSON export file")
                                    onClicked: id_achievementImportDialog.open()
                                }

                                CustomButton {
                                    text: qsTr("Export")
                                    p_tooltipText: qsTr("Export Emulator achievement data to a JSON file")
                                    onClicked: id_achievementExportDialog.open()
                                }
                            }
                        }

                        C_SettingRow {
                            visible: id_root.achievementTransportStatus !== ""
                            Layout.fillWidth: true
                            Layout.columnSpan: 2

                            Label {
                                Layout.fillWidth: true
                                text: id_root.achievementTransportStatus
                                color: Themes.settings.colors.sectionInfo
                                font.pixelSize: Themes.settings.fontSizes.sectionInfo
                                wrapMode: Text.WordWrap
                            }
                        }

                    }

                    // Defaults
                    C_SettingsSection {
                        fullRowMode: true

                        C_SettingRow {
                            RowLayout {
                                spacing: 8

                                CustomButton {
                                    text: qsTr("License")
                                    onClicked: id_markdownDocumentPopup.openDocument(qsTr("Lymalink License"), LICENSE_MD_TEXT)
                                }

                                CustomButton {
                                    text: qsTr("Third-Party Licenses")
                                    onClicked: id_markdownDocumentPopup.openDocument(qsTr("Third-Party Licenses"), THIRD_PARTY_LICENSES_MD_TEXT)
                                }

                                CustomButton {
                                    text: qsTr("Credits")
                                    onClicked: id_markdownDocumentPopup.openDocument(qsTr("Credits"), CREDITS_MD_TEXT)
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            CustomButton {
                                text: qsTr("User Guide")
                                onClicked: id_markdownDocumentPopup.openDocument(qsTr("User Guide"), USER_GUIDE_MD_TEXT)
                            }

                            CustomButton {
                                Layout.preferredWidth: 140
                                text: qsTr("Reset Defaults")
                                p_tooltipText: qsTr("Reset all settings and restore the default window size")

                                onClicked: {
                                    if (ctxSettings.ResetDefaults() && id_root.backendServiceReady) {
                                        ctxBackendService.ReloadConfig()
                                    }

                                    // Also reset window size
                                    const win = id_root.Window.window
                                    if (!win) {
                                        return
                                    }
                                    win.showNormal()
                                    win.width = ctxSettings.windowSizeXDefault
                                    win.height = ctxSettings.windowSizeYDefault
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.preferredHeight: 48
                }
            }
        }
    }
}
