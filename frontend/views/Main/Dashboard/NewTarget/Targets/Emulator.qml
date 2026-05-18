/////////////////////////////////////////////////////////
// File: Emulator.qml
// Date: 2026-05-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Emulator Target for active tracking
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: id_root

    // Internals _____________________________________________
    property var searchResults: []
    property bool isSearching: false
    property string statusText: ""
    property int selectedAppId: 0
    property string selectedName: ""
    property int addedDate: Math.floor(Date.now() / 1000)
    property bool manualGameEntry: false
    property bool manualAchievementPath: false

    function fileUrlToPath(fileUrl) {
        return decodeURIComponent(fileUrl.toString().replace("file://", ""))
    }

    function searchGames() {
        const term = id_searchField.text.trim()
        if (term.length === 0) {
            id_root.statusText = qsTr("Enter game name")
            id_root.searchResults = []
            return
        }

        id_root.isSearching = true
        id_root.searchResults = ctxLymalink.SearchSteamAppIds(term)
        id_root.isSearching = false
        id_root.statusText = id_root.searchResults.length === 0
            ? qsTr("No results")
            : (id_root.searchResults.length === 10 ? qsTr("%1 results - Limited to max 10 results").arg(id_root.searchResults.length) : qsTr("%1 results").arg(id_root.searchResults.length))
    }

    function selectGame(result) {
        id_root.selectedAppId = result.id
        id_root.selectedName = result.name
        id_root.addedDate = Math.floor(Date.now() / 1000)
    }

    function setManualGameEntry(enabled) {
        id_root.manualGameEntry = enabled
        id_root.searchResults = []
        id_root.statusText = ""
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    FileDialog {
        id: id_exeFileDialog
        title: qsTr("Select Game Executable")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Executable files (*.exe)")]
        onAccepted: id_installLocationField.text = id_root.fileUrlToPath(selectedFile)
    }

    FileDialog {
        id: id_achievementFileDialog
        title: qsTr("Select Achievement File")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("All files (*)")]
        onAccepted: {
            id_root.manualAchievementPath = true
            id_achievementLocationField.text = id_root.fileUrlToPath(selectedFile)
        }
    }

    ScrollView {
        id: id_scrollView

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            id: id_content

            readonly property int sideMargin: 20
            
            x: Math.max(sideMargin, (id_scrollView.availableWidth - width) / 2)
            width: Math.max(0, Math.min(id_scrollView.availableWidth - sideMargin * 2, 760))

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.topMargin: 20
                    text: qsTr("1. Search game")
                    font.pixelSize: Themes.emulatorTarget.fontSizes.title
                    font.bold: true
                    color: Themes.emulatorTarget.colors.titleText
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: !id_root.manualGameEntry

                    TextField {
                        id: id_searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Game name")
                        font.pixelSize: Themes.emulatorTarget.fontSizes.input
                        onAccepted: {
                            id_root.searchResults = []
                            id_root.searchGames()
                        }
                    }

                    Button {
                        text: qsTr("Search")
                        enabled: !id_root.isSearching
                        onClicked: {
                            id_root.searchResults = []
                            id_root.searchGames()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        visible: !id_root.manualGameEntry
                        Layout.fillWidth: true
                        text: id_root.statusText
                        font.pixelSize: Themes.emulatorTarget.fontSizes.description
                        color: Themes.emulatorTarget.colors.descriptionText
                    }

                    Text {
                        text: qsTr("enter manually")
                        font.pixelSize: Themes.emulatorTarget.fontSizes.description
                        color: Themes.emulatorTarget.colors.descriptionText
                    }

                    CheckBox {
                        checked: id_root.manualGameEntry
                        onToggled: id_root.setManualGameEntry(checked)
                    }
                }

                CustomBusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    visible: running
                    indicatorSize: 40
                    running: !id_root.manualGameEntry && id_root.isSearching
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: !id_root.manualGameEntry

                    Repeater {
                        model: id_root.searchResults

                        delegate: Rectangle {
                            id: id_resultRow
                            Layout.fillWidth: true
                            height: 44
                            radius: 6
                            color: id_resultMouseArea.pressed
                                ? Themes.emulatorTarget.colors.resultBackgroundPressed
                                : id_resultMouseArea.containsMouse
                                    ? Themes.emulatorTarget.colors.resultBackgroundHover
                                    : Themes.emulatorTarget.colors.resultBackground
                            border.width: 1
                            border.color: id_root.selectedAppId === modelData.id
                                ? Themes.emulatorTarget.colors.resultBorderSelected
                                : Themes.emulatorTarget.colors.resultBorder

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                Text {
                                    text: modelData.id
                                    font.pixelSize: Themes.emulatorTarget.fontSizes.description
                                    color: Themes.emulatorTarget.colors.descriptionText
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    elide: Text.ElideRight
                                    font.pixelSize: Themes.emulatorTarget.fontSizes.label
                                    color: Themes.emulatorTarget.colors.labelText
                                }
                            }

                            MouseArea {
                                id: id_resultMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: id_root.selectGame(modelData)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Themes.emulatorTarget.colors.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                enabled: id_root.manualGameEntry || id_root.selectedAppId > 0
                opacity: enabled ? 1.0 : 0.45

                Text {
                    Layout.topMargin: 20
                    text: qsTr("2. Target details")
                    font.pixelSize: Themes.emulatorTarget.fontSizes.title
                    font.bold: true
                    color: Themes.emulatorTarget.colors.titleText
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 10

                    Text {
                        Layout.preferredWidth: 180
                        text: qsTr("ID")
                        color: Themes.emulatorTarget.colors.descriptionText
                        font.pixelSize: Themes.emulatorTarget.fontSizes.label
                        wrapMode: Text.Wrap
                    }

                    TextField {
                        id: id_appIdField
                        Layout.fillWidth: true
                        readOnly: !id_root.manualGameEntry
                        selectByMouse: id_root.manualGameEntry
                        focusPolicy: id_root.manualGameEntry ? Qt.StrongFocus : Qt.NoFocus
                        text: id_root.selectedAppId > 0 ? id_root.selectedAppId.toString() : ""
                        validator: IntValidator { bottom: 1 }
                        onTextEdited: id_root.selectedAppId = text.length > 0 ? parseInt(text) : 0
                    }

                    Text {
                        Layout.preferredWidth: 180
                        text: qsTr("Name")
                        color: Themes.emulatorTarget.colors.descriptionText
                        font.pixelSize: Themes.emulatorTarget.fontSizes.label
                        wrapMode: Text.Wrap
                    }

                    TextField {
                        Layout.fillWidth: true
                        readOnly: !id_root.manualGameEntry
                        selectByMouse: id_root.manualGameEntry
                        focusPolicy: id_root.manualGameEntry ? Qt.StrongFocus : Qt.NoFocus
                        text: id_root.selectedName
                        onTextEdited: id_root.selectedName = text
                    }

                    Text {
                        Layout.preferredWidth: 180
                        text: qsTr("Game Executable Location")
                        color: Themes.emulatorTarget.colors.descriptionText
                        font.pixelSize: Themes.emulatorTarget.fontSizes.label
                        wrapMode: Text.Wrap
                    }

                    TextField {
                        id: id_installLocationField
                        Layout.fillWidth: true
                        readOnly: true
                        selectByMouse: false
                        placeholderText: qsTr("Full path to game executable (.exe)")

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: id_exeFileDialog.open()
                        }
                    }

                    Text {
                        Layout.preferredWidth: 180
                        text: qsTr("Achievement File Location")
                        color: Themes.emulatorTarget.colors.descriptionText
                        font.pixelSize: Themes.emulatorTarget.fontSizes.label
                        wrapMode: Text.Wrap
                    }

                    TextField {
                        id: id_achievementLocationField
                        Layout.fillWidth: true
                        enabled: id_root.manualAchievementPath
                        readOnly: true
                        selectByMouse: false
                        text: "auto"
                        placeholderText: qsTr("Manual path override")

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: id_achievementFileDialog.open()
                        }
                    }

                    Text {
                        text: ""
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        CheckBox {
                            text: qsTr("Manual achievement file path")
                            checked: id_root.manualAchievementPath
                            focusPolicy: Qt.NoFocus  
                            onToggled: {
                                id_root.manualAchievementPath = checked
                                if (!checked) {
                                    id_achievementLocationField.text = "auto"
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            id: id_confirmTarget

                            readonly property bool canConfirm: id_root.selectedAppId > 0
                                && id_root.selectedName.trim().length > 0
                                && id_installLocationField.text.trim().length > 0

                            implicitHeight: 32
                            implicitWidth: id_confirmTargetLabel.implicitWidth + 60
                            radius: 6
                            enabled: canConfirm
                            opacity: enabled ? 1.0 : 0.45

                            color: id_confirmTargetMouseArea.pressed
                                ? Themes.emulatorTarget.colors.confirmBackgroundPressed
                                : id_confirmTargetMouseArea.containsMouse
                                    ? Themes.emulatorTarget.colors.confirmBackgroundHover
                                    : Themes.emulatorTarget.colors.confirmBackground

                            border.width: 1
                            border.color: id_confirmTargetMouseArea.pressed
                                ? Themes.emulatorTarget.colors.confirmBorderPressed
                                : id_confirmTargetMouseArea.containsMouse
                                    ? Themes.emulatorTarget.colors.confirmBorderHover
                                    : Themes.emulatorTarget.colors.confirmBorder

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Text {
                                id: id_confirmTargetLabel
                                anchors.centerIn: parent
                                text: qsTr("Confirm")
                                color: Themes.emulatorTarget.colors.confirmText
                                font.pixelSize: Themes.emulatorTarget.fontSizes.confirmButton
                            }

                            MouseArea {
                                id: id_confirmTargetMouseArea
                                anchors.fill: parent
                                enabled: id_confirmTarget.canConfirm
                                hoverEnabled: enabled
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }
                }
            }
        }
    }
}
