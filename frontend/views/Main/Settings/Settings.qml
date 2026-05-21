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
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: id_root

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
            text: id_rowRoot.label
            color: Themes.settings.colors.labelText
            font.pixelSize: Themes.settings.fontSizes.labelText
            
            Layout.fillWidth: id_rowRoot.fixedWidthInt === 0
            Layout.minimumWidth: fixedWidthInt !== 0 ? fixedWidthInt / 5 : 0
            
            elide: Text.ElideRight

            HoverHandler {
                id: id_labelHover
            }

            CustomTooltip {
                active: id_rowRoot.tooltip !== "" && id_labelHover.hovered
                delay: 600
                text: id_rowRoot.tooltip
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

        Button {
            text: qsTr("Apply")
            onClicked: {
                const submittedText = id_input.text
                id_maskedInputRoot.applyClicked(submittedText)

                if (id_maskedInputRoot.enableMasking && submittedText.length > 0) {
                    id_input.text = ""
                    id_inputFrame.masked = true
                    id_flashAnim.restart()
                } else {
                    id_inputFrame.masked = false
                    id_flashAnim.restart()
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
            anchors.rightMargin: Math.max(60, parent.width - 40 - 820)

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
        anchors.top: id_fixedHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        contentHeight: id_contentLayout.height
        clip: true

        Item {
            width: parent.width
            height: id_contentLayout.implicitHeight

            ColumnLayout {
                id: id_contentLayout

                // Pin to left with fixed margins, cap width on the right
                anchors.left: parent.left
                anchors.leftMargin: 40
                anchors.right: parent.right
                anchors.rightMargin: Math.max(60, parent.width - 40 - 820)

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
                            ComboBox {
                                model: ["system", "dark", "light"]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.theme))
                                implicitWidth: 140
                                displayText: {
                                    switch (model[currentIndex]) {
                                        case "system": return qsTr("System")
                                        case "dark": return qsTr("Dark")
                                        case "light": return qsTr("Light")
                                    }
                                }
                                delegate: ItemDelegate {
                                    width: parent.width
                                    text: {
                                        switch (modelData) {
                                            case "system": return qsTr("System")
                                            case "dark": return qsTr("Dark")
                                            case "light": return qsTr("Light")
                                        }
                                    }
                                    enabled: modelData === "dark"
                                }
                                onActivated: (index) => ctxSettings.SaveValue(Settings.Theme, model[index])
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Lymalink Logo")
                            tooltip: qsTr("Show or hide the Lymalink logo in the sidebar")
                            Switch {
                                checked: ctxSettings.showLymalinkLogo
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_logoSwitchHover }
                                CustomTooltip {
                                    active: id_logoSwitchHover.hovered
                                    delay: 600
                                    text: qsTr("Show or hide the Lymalink logo in the sidebar")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowLymalinkLogo, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Language")
                            tooltip: qsTr("Sets the application's display language")
                            ComboBox {
                                model: ["English", "Finnish", "Svenska"]
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.language))
                                implicitWidth: 140
                                displayText: model[currentIndex]
                                delegate: ItemDelegate {
                                    width: parent.width
                                    text: modelData
                                    enabled: modelData === "English"
                                }
                                onActivated: (index) => ctxSettings.SaveValue(Settings.Language, model[index])
                            }
                        }

                        C_SettingRow {}

                        C_SettingRow {
                            label: qsTr("Window size")
                            tooltip: qsTr("Reset the main window size to its default dimensions")

                            Button {
                                Layout.preferredWidth: 140
                                text: qsTr("Reset to default")
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
                            Switch {
                                checked: ctxSettings.closeToTray
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_trayHover }
                                CustomTooltip {
                                    active: id_trayHover.hovered
                                    delay: 600
                                    text: qsTr("When closing the window, minimize the application to the system tray instead of exiting")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.CloseToTray, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse button")
                            tooltip: qsTr("Show a button for collapsing the sidebar")
                            Switch {
                                checked: ctxSettings.showCollapseButton
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_collapseButtonHover }
                                CustomTooltip {
                                    active: id_collapseButtonHover.hovered
                                    delay: 600
                                    text: qsTr("Show a button for collapsing the sidebar")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowCollapseButton, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Close to tray notification")
                            tooltip: qsTr("Show system notification while closing to tray")
                            Switch {
                                enabled: ctxSettings.closeToTray
                                checked: ctxSettings.closeToTrayToast && enabled
                                text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_trayToastHover }
                                CustomTooltip {
                                    active: id_trayToastHover.hovered
                                    delay: 600
                                    text: qsTr("Show system notification while closing to tray")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.CloseToTrayToast, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse border button")
                            tooltip: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                            Switch {
                                checked: ctxSettings.enableCollapseBorderButton
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_collapseBorderHover }
                                CustomTooltip {
                                    active: id_collapseBorderHover.hovered
                                    delay: 600
                                    text: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableCollapseBorderButton, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Tooltips")
                            tooltip: qsTr("Show tooltips")
                            Switch {
                                checked: ctxSettings.showTooltips
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_tooltipsHover }
                                CustomTooltip {
                                    active: id_tooltipsHover.hovered
                                    delay: 600
                                    text: qsTr("Show tooltips")
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
                            ComboBox {
                                model: [0, 1, 2, 3, 4, 5]
                                enabled: ctxSettings.showProgressFrame && !ctxSettings.progressFrameGrayscaleMode
                                currentIndex: Math.max(0, model.indexOf(ctxSettings.globalColorStyle))
                                implicitWidth: 150
                                displayText: {
                                    switch (model[currentIndex]) {
                                        case 0: return qsTr("Gold")
                                        case 1: return qsTr("Blue")
                                        case 2: return qsTr("Purple")
                                        case 3: return qsTr("Emerald")
                                        case 4: return qsTr("Ember")
                                        case 5: return qsTr("Frost")
                                    }
                                }
                                delegate: ItemDelegate {
                                    width: parent.width
                                    text: {
                                        switch (modelData) {
                                            case 0: return qsTr("Gold")
                                            case 1: return qsTr("Blue")
                                            case 2: return qsTr("Purple")
                                            case 3: return qsTr("Emerald")
                                            case 4: return qsTr("Ember")
                                            case 5: return qsTr("Frost")
                                        }
                                    }
                                }
                                onActivated: (index) => ctxSettings.SaveValue(Settings.GlobalColorStyle, model[index])
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame")
                            tooltip: qsTr("Show an overall achievement progress frame around cards")
                            Switch {
                                checked: ctxSettings.showProgressFrame
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_progressFrameHover }
                                CustomTooltip {
                                    active: id_progressFrameHover.hovered
                                    delay: 600
                                    text: qsTr("Show an overall achievement progress frame around cards")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowProgressFrame, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame grayscale mode")
                            tooltip: qsTr("Render the progress frame in grayscale instead of color - disables animations")
                            Switch {
                                enabled: ctxSettings.showProgressFrame
                                checked: ctxSettings.progressFrameGrayscaleMode && enabled
                                text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_progressGrayHover }
                                CustomTooltip {
                                    active: id_progressGrayHover.hovered
                                    delay: 600
                                    text: qsTr("Render the progress frame in grayscale instead of color - disables animations")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ProgressFrameGrayscaleMode, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame completion animation")
                            tooltip: qsTr("Play a subtle breath animation on completed card progress frame - not available in grayscale mode")
                            Switch {
                                enabled: ctxSettings.showProgressFrame && !ctxSettings.progressFrameGrayscaleMode
                                checked: ctxSettings.enableProgressFrameCompletionAnimation && enabled
                                text: checked && enabled ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_progressAnimHover }
                                CustomTooltip {
                                    active: id_progressAnimHover.hovered
                                    delay: 600
                                    text: qsTr("Play a subtle breath animation on completed card progress frame - not available in grayscale mode")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableProgressFrameCompletionAnimation, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Total achievements badge")
                            tooltip: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                            Switch {
                                checked: ctxSettings.showTotalAchievementsBadge
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_achieveBadgeHover }
                                CustomTooltip {
                                    active: id_achieveBadgeHover.hovered
                                    delay: 600
                                    text: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowTotalAchievementsBadge, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Installation status badge")
                            tooltip: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                            Switch {
                                checked: ctxSettings.showInstallationStatusBadge
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_installIconHover }
                                CustomTooltip {
                                    active: id_installIconHover.hovered
                                    delay: 600
                                    text: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowInstallationStatusBadge, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Dynamic achievement rows")
                            tooltip: qsTr("Achievement rows resize automatically to use available window space")
                            Switch {
                                checked: ctxSettings.enableDynamicAchievementRows
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_dynamicAchievementRows }
                                CustomTooltip {
                                    active: id_dynamicAchievementRows.hovered
                                    delay: 600
                                    text: qsTr("Achievement rows resize automatically to use available window space")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.EnableDynamicAchievementRows, checked)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Target type badge")
                            tooltip: qsTr("Show a badge on cards indicating whether the target is Custom, Steam, or Emulator")
                            Switch {
                                checked: ctxSettings.showTargetTypeBadge
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_targetTypeBadgeHover }
                                CustomTooltip {
                                    active: id_targetTypeBadgeHover.hovered
                                    delay: 600
                                    text: qsTr("Show a badge on cards indicating whether the target is Custom, Steam, or Emulator")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.ShowTargetTypeBadge, checked)
                            }
                        }
                    }

                    // Backend Service
                    C_SettingsSection {
                        fullRowMode: true
                        title: qsTr("Backend Service")
                        infoText: qsTr("Controls whether Lymalink runs achievement tracking in the background.\n\nWhen enabled, a system service is registered and kept running independently. Tracking and notifications continue even when this application is closed. When disabled, tracking runs only while the application is open, and the service is stopped when the application exits.\n\nSidebar indicator: green = service running independently, yellow = tracking requires the application to stay open, red = service error.")

                        C_SettingRow {
                            label: qsTr("Background service")
                            tooltip: qsTr("Keep tracking active even when the application is closed")
                            fixedWidthInt: 500

                            Switch {
                                checked: ctxSettings.backendService
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                HoverHandler { id: id_backendServiceHover }
                                CustomTooltip {
                                    active: id_backendServiceHover.hovered
                                    delay: 600
                                    text: qsTr("Keep tracking active even when the application is closed")
                                }
                                onToggled: ctxSettings.SaveValue(Settings.BackendService, checked)
                            }
                        }
                    }

                    // Steam API
                    C_SettingsSection {
                        fullRowMode: true
                        title: qsTr("Steam Web API")
                        infoText: qsTr("Steam Web API can be used to import your Steam achievement progress into Lymalink\n\nNote: Currently, API key is saved to the local config file using the default encryption key, which is not secure for long-term storage. This will be improved in a future update.")

                        C_SettingRow {
                            label: qsTr("Steam ID")
                            tooltip: qsTr("Steam ID is a long numeric account identifier - You can find it on your Steam Account page")
                            fixedWidthInt: 500

                            C_ApplyInput {
                                inputText: ctxSettings.steamId
                                onApplyClicked: (text) => ctxSettings.SaveValue(Settings.SteamId, text)
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Web API Key")
                            tooltip: qsTr("Get your Steam Web API key from https://steamcommunity.com/dev/apikey")
                            fixedWidthInt: 500

                            C_ApplyInput {
                                enableMasking: true
                                initiallyMasked: ctxSettings.steamWebApiKey !== ""
                                onApplyClicked: (text) => {
                                    if (text.length > 0 || text === "") {
                                        let val = text === "" ? "reset" : text
                                        ctxSettings.SaveValue(Settings.SteamWebApiKey, val)
                                    }
                                }
                            }
                        }
                    }

                    // Defaults
                    C_SettingsSection {
                        C_SettingRow {
                            Button {
                                Layout.preferredWidth: 140
                                text: qsTr("Reset Defaults")
                                onClicked: {
                                    ctxSettings.ResetDefaults()

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
