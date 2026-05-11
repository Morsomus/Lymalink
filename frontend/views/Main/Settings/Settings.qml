/////////////////////////////////////////////////////////
// File: Settings.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Settings page.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: id_root

    // Section header component
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

    // Row for Single Settings Option
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

            ToolTip.visible: id_rowRoot.tooltip !== "" && id_labelHover.hovered
            ToolTip.text: id_rowRoot.tooltip
            ToolTip.delay: 600

            HoverHandler {
                id: id_labelHover
            }
        }
    }

    // Input field
    component C_ApplyInput: RowLayout {
        id: id_maskedInputRoot

        property alias inputText: id_input.text
        property bool enableMasking: false
        property string maskedText: "**********"
        property int fieldWidth: 280
        property int fieldHeight: 32
        property int flashDuration: 550

        spacing: 8

        Rectangle {
            id: id_inputFrame
            property bool masked: false
            property real flashOpacity: 0.0

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
                if (id_maskedInputRoot.enableMasking && id_input.text.length > 0) {
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
                                currentIndex: 1
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
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Lymalink Logo")
                            tooltip: qsTr("Show or hide the Lymalink logo in the sidebar")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_logoSwitchHover.hovered
                                ToolTip.text: qsTr("Show or hide the Lymalink logo in the sidebar")
                                ToolTip.delay: 600
                                HoverHandler { id: id_logoSwitchHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Language")
                            tooltip: qsTr("Sets the application's display language")
                            ComboBox {
                                model: ["English", "Finnish", "Svenska"]
                                currentIndex: 0
                                implicitWidth: 140
                                displayText: model[currentIndex]
                                delegate: ItemDelegate {
                                    width: parent.width
                                    text: modelData
                                    enabled: modelData === "English"
                                }
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
                                    win.width = 1510
                                    win.height = 900
                                }
                            }
                        }        
                    }

                    // Interface
                    C_SettingsSection {
                        title: qsTr("Interface")

                        C_SettingRow {
                            label: qsTr("Minimize to tray")
                            tooltip: qsTr("When closing the window, minimize the application to the system tray instead of exiting")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_trayHover.hovered
                                ToolTip.text: qsTr("When closing the window, minimize the application to the system tray instead of exiting")
                                ToolTip.delay: 600
                                HoverHandler { id: id_trayHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse button")
                            tooltip: qsTr("Show a button for collapsing the sidebar")
                            Switch {
                                checked: false
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_collapseButtonHover.hovered
                                ToolTip.text: qsTr("Show a button for collapsing the sidebar")
                                ToolTip.delay: 600
                                HoverHandler { id: id_collapseButtonHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Tooltips")
                            tooltip: qsTr("Show tooltips")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_tooltipsHover.hovered
                                ToolTip.text: qsTr("Show tooltips")
                                ToolTip.delay: 600
                                HoverHandler { id: id_tooltipsHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Collapse border button")
                            tooltip: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_collapseBorderHover.hovered
                                ToolTip.text: qsTr("Enable a hidden hover button on the edge of the sidebar for collapsing it")
                                ToolTip.delay: 600
                                HoverHandler { id: id_collapseBorderHover }
                            }
                        }
                    }

                    // Display
                    C_SettingsSection {
                        title: qsTr("Display")

                        C_SettingRow {
                            label: qsTr("Progress frame")
                            tooltip: qsTr("Show an overall achievement progress frame around cards")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_progressFrameHover.hovered
                                ToolTip.text: qsTr("Show an overall achievement progress frame around cards")
                                ToolTip.delay: 600
                                HoverHandler { id: id_progressFrameHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Installation status badge")
                            tooltip: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_installIconHover.hovered
                                ToolTip.text: qsTr("Show a warning badge in the top-left corner of a card if the installation cannot be found")
                                ToolTip.delay: 600
                                HoverHandler { id: id_installIconHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame grayscale mode")
                            tooltip: qsTr("Render the progress frame in grayscale instead of color - disables animations")
                            Switch {
                                checked: false
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_progressGrayHover.hovered
                                ToolTip.text: qsTr("Render the progress frame in grayscale instead of color - disables animations")
                                ToolTip.delay: 600
                                HoverHandler { id: id_progressGrayHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Total achievements badge")
                            tooltip: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_achieveBadgeHover.hovered
                                ToolTip.text: qsTr("Show a badge in the top-right corner of a card displaying the total number of achievements")
                                ToolTip.delay: 600
                                HoverHandler { id: id_achieveBadgeHover }
                            }
                        }

                        C_SettingRow {
                            label: qsTr("Progress frame completion animation")
                            tooltip: qsTr("Play a subtle animation on completed card progress frame - not available in grayscale mode")
                            Switch {
                                checked: true
                                text: checked ? qsTr("Enabled") : qsTr("Disabled")
                                ToolTip.visible: id_progressAnimHover.hovered
                                ToolTip.text: qsTr("Play a subtle animation on completed card progress frame - not available in grayscale mode")
                                ToolTip.delay: 600
                                HoverHandler { id: id_progressAnimHover }
                            }
                        }
                    }

                    // Steam API
                    C_SettingsSection {
                        fullRowMode: true
                        title: qsTr("Steam Web API")
                        infoText: qsTr("Steam Web API can be used to import your Steam achievement progress into Lymalink")

                        C_SettingRow {
                            label: qsTr("Steam ID")
                            tooltip: qsTr("Steam ID is a long numeric account identifier - You can find it on your Steam Account page")
                            fixedWidthInt: 500

                            C_ApplyInput {}
                        }

                        C_SettingRow {
                            label: qsTr("Web API Key")
                            tooltip: qsTr("Get your Steam Web API key from https://steamcommunity.com/dev/apikey")
                            fixedWidthInt: 500

                            C_ApplyInput { enableMasking: true }
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
