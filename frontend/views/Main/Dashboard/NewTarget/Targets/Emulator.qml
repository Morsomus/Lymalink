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

    // Public ________________________________________________
    signal targetAdded(int appId)

    // Internals _____________________________________________
    property var searchResults: []
    property bool isSearching: false
    property bool suppressNextCancelStatus: false
    property string statusText: ""
    property bool statusIsError: false
    property string targetStatusText: ""
    property bool targetStatusIsError: false
    property bool isCreatingTarget: false
    property int selectedAppId: 0
    property string selectedName: ""
    property int addedDate: Math.floor(Date.now() / 1000)
    property bool manualGameEntry: false
    readonly property color themedProgressColor: Themes.globalStyle.progressColor(ctxSettings.globalColorStyle)
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)
    readonly property bool prefixWarning: {
        const t = id_prefixLocationField.text.trim()
        return t.length > 0 && !t.split("/").pop().startsWith("drive_")
    }

    Connections {
        target: ctxLymalink

        function onSignalSteamAppIdsReady(success, cancelled, results) {
            if (!id_root.isSearching && !cancelled) {
                return
            }

            id_root.isSearching = false

            if (cancelled) {
                if (id_root.suppressNextCancelStatus) {
                    id_root.suppressNextCancelStatus = false
                    return
                }

                id_root.searchResults = []
                id_root.statusText = qsTr("Search cancelled")
                id_root.statusIsError = false
                return
            }

            id_root.searchResults = results
            id_root.statusIsError = !success
            id_root.statusText = !success
                ? qsTr("Couldn't connect to API service")
                : (id_root.searchResults.length === 0
                    ? qsTr("No results")
                    : (id_root.searchResults.length === 10
                        ? qsTr("%1 results - Limited to max 10 results").arg(id_root.searchResults.length)
                        : qsTr("%1 results").arg(id_root.searchResults.length)))
        }
    }

    Component.onDestruction: id_root.cancelSearch(false)

    function fileUrlToPath(fileUrl) {
        return decodeURIComponent(fileUrl.toString().replace("file://", ""))
    }

    function searchGames() {
        if (id_root.isSearching) {
            return
        }

        const term = id_searchField.text.trim()
        if (term.length === 0) {
            id_root.statusText = qsTr("Enter game name")
            id_root.statusIsError = false
            id_root.searchResults = []
            return
        }

        id_root.isSearching = true
        id_root.suppressNextCancelStatus = false
        id_root.statusIsError = false
        id_root.statusText = qsTr("Searching...")
        id_root.searchResults = []
        ctxLymalink.SearchSteamAppIds(term)
    }

    function cancelSearch(showStatus) {
        if (!id_root.isSearching) {
            return
        }

        id_root.isSearching = false
        id_root.suppressNextCancelStatus = !showStatus
        ctxLymalink.CancelSteamAppIdSearch()

        if (showStatus) {
            id_root.statusText = qsTr("Search cancelled")
            id_root.statusIsError = false
        }
    }

    function selectGame(result) {
        id_root.selectedAppId = result.id
        id_root.selectedName = result.name
        id_root.addedDate = Math.floor(Date.now() / 1000)
        id_root.targetStatusText = ""
        id_root.targetStatusIsError = false
    }

    function setManualGameEntry(enabled) {
        id_root.cancelSearch(false)
        id_root.manualGameEntry = enabled
        id_root.searchResults = []
        id_root.statusText = ""
        id_root.statusIsError = false
        id_root.targetStatusText = ""
        id_root.targetStatusIsError = false
    }

    function createTarget() {
        if (id_root.isCreatingTarget || !id_confirmTarget.canConfirm) {
            return
        }

        id_root.isCreatingTarget = true
        id_root.targetStatusText = qsTr("Creating target...")
        id_root.targetStatusIsError = false

        const success = ctxLymalink.CreateNewSteamEmuTarget(
            id_root.selectedAppId,
            id_root.selectedName,
            id_installLocationField.text,
            id_prefixLocationField.text
        )

        id_root.isCreatingTarget = false
        id_root.targetStatusIsError = !success
        id_root.targetStatusText = success
            ? qsTr("Target created")
            : qsTr("Couldn't create target. Emulator target with this ID " + id_root.selectedAppId + " may already exist.")

        if (success) {
            id_root.targetAdded(id_root.selectedAppId)
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    FileDialog {
        id: id_exeFileDialog

        title: qsTr("Select Game Executable")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Executable files (*.exe)")]
        onAccepted: {
            id_installLocationField.text = id_root.fileUrlToPath(selectedFile)
            id_root.targetStatusText = ""
            id_root.targetStatusIsError = false
        }
    }

    FolderDialog {
        id: id_prefixFolderDialog

        title: qsTr("Select Prefix Location (drive_c or equivalent)")
        onAccepted: {
            id_prefixLocationField.text = id_root.fileUrlToPath(selectedFolder)
            id_root.targetStatusText = ""
            id_root.targetStatusIsError = false
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

            // Info block
            Rectangle {
                id: id_infoBlock

                property bool hoverActive: false
                Layout.fillWidth: true
                Layout.topMargin: 20
                radius: 6
                color: id_infoBlock.hoverActive
                    ? Themes.emulatorTarget.colors.infoBlockBackgroundHover
                    : Themes.emulatorTarget.colors.infoBlockBackground
                border.width: 1
                border.color: id_infoBlock.hoverActive
                    ? Themes.emulatorTarget.colors.infoBlockBorderHover
                    : Themes.emulatorTarget.colors.infoBlockBorder

                implicitHeight: id_infoBlock.hoverActive
                    ? id_infoHeaderRow.implicitHeight + id_infoExpandable.implicitHeight + 32
                    : id_infoHeaderRow.implicitHeight + 28

                Behavior on implicitHeight {
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.InOutQuad
                    }
                }
                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                Timer {
                    id: id_infoBlockHoverTimer
                    interval: 200
                    repeat: false
                    onTriggered: id_infoBlock.hoverActive = true
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.ArrowCursor
                    onEntered: id_infoBlockHoverTimer.start()
                    onExited: {
                        id_infoBlockHoverTimer.stop()
                        id_infoBlock.hoverActive = false
                    }
                }

                ColumnLayout {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 14
                    }
                    spacing: 0

                    // Info Header row
                    RowLayout {
                        id: id_infoHeaderRow
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 16
                            height: 16
                            radius: 8
                            color: "transparent"
                            border.width: 1
                            border.color: Themes.emulatorTarget.colors.infoIconBorder

                            Text {
                                anchors.centerIn: parent
                                text: "i"
                                font.pixelSize: Themes.emulatorTarget.fontSizes.infoIcon
                                font.italic: true
                                font.bold: true
                                color: Themes.emulatorTarget.colors.infoIconText
                            }
                        }

                        Text {
                            text: qsTr("How detection works")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.label
                            font.bold: true
                            color: id_infoBlock.hoverActive
                                ? Themes.emulatorTarget.colors.labelText
                                : Themes.emulatorTarget.colors.infoHeaderInactiveText

                            Behavior on color {
                                ColorAnimation { duration: 120 }
                            }
                        }
                    }

                    // Expandable content
                    ColumnLayout {
                        id: id_infoExpandable

                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        spacing: 6
                        opacity: id_infoBlock.hoverActive ? 1.0 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 140
                                easing.type: Easing.InOutQuad
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Detection is designed to run only when needed. The backend scans for achievement data only while the selected game is active, avoiding unnecessary background activity.")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.description
                            color: Themes.emulatorTarget.colors.descriptionText
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("The backend monitors the game executable to detect when the game is running. Playtime tracking and achievement scanning are both tied to the process. Neither runs unless the game is active.")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.description
                            color: Themes.emulatorTarget.colors.descriptionText
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("The prefix location is used to locate achievement data written by the emulator. Once the backend finds achievement data matching the selected game ID inside the prefix, the path is saved. Future scans access that location directly without searching the prefix again.")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.description
                            color: Themes.emulatorTarget.colors.descriptionText
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            // Section 1: Search game
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.topMargin: 20
                    text: qsTr("1. Search game info")
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
                        color: id_root.statusIsError
                            ? Themes.emulatorTarget.colors.errorText
                            : Themes.emulatorTarget.colors.descriptionText
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
                            color: id_resultRowMouseArea.pressed
                                ? Themes.emulatorTarget.colors.resultBackgroundPressed
                                : id_resultRowMouseArea.containsMouse
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
                                id: id_resultRowMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    id_root.selectGame(modelData)
                                    id_searchField.focus = false
                                }
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

            // Section 2: Target details
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

                    // ID
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
                        onTextEdited: {
                            id_root.selectedAppId = text.length > 0 ? parseInt(text) : 0
                            id_root.targetStatusText = ""
                            id_root.targetStatusIsError = false
                        }
                    }

                    // Name
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
                        onTextEdited: {
                            id_root.selectedName = text
                            id_root.targetStatusText = ""
                            id_root.targetStatusIsError = false
                        }
                    }

                    // Game Executable
                    ColumnLayout {
                        Layout.preferredWidth: 180
                        spacing: 2

                        Text {
                            text: qsTr("Game Executable")
                            color: Themes.emulatorTarget.colors.descriptionText
                            font.pixelSize: Themes.emulatorTarget.fontSizes.label
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Used to track activity")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.descriptionSubtle
                            color: Themes.emulatorTarget.colors.descriptionMutedText
                            wrapMode: Text.Wrap
                        }
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

                    // Prefix Location
                    ColumnLayout {
                        Layout.preferredWidth: 180
                        spacing: 2

                        Text {
                            text: qsTr("Prefix Location")
                            color: Themes.emulatorTarget.colors.descriptionText
                            font.pixelSize: Themes.emulatorTarget.fontSizes.label
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Select the drive_c or equivalent folder\ninside your Wine/Proton/Bottle prefix")
                            font.pixelSize: Themes.emulatorTarget.fontSizes.descriptionSubtle
                            color: Themes.emulatorTarget.colors.descriptionMutedText
                            wrapMode: Text.Wrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        TextField {
                            id: id_prefixLocationField

                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: false
                            placeholderText: qsTr("e.g. /home/user/prefix/drive_c")

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: id_prefixFolderDialog.open()
                            }
                        }

                        Text {
                            visible: id_root.prefixWarning
                            Layout.fillWidth: true
                            text: qsTr("⚠ Expected a drive_c or equivalent folder. Make sure the selected folder contains or will contain a subdirectory matching the game ID (%1). This is required for achievement detection to work.").arg(id_root.selectedAppId > 0
                                ? id_root.selectedAppId.toString()
                                : qsTr("not set"))
                            font.pixelSize: Themes.emulatorTarget.fontSizes.description
                            color: Themes.emulatorTarget.colors.prefixWarningText
                            wrapMode: Text.Wrap
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // Confirm row
                    RowLayout {
                        spacing: 12
                        Layout.topMargin: 4

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            Layout.preferredWidth: 420
                            Layout.maximumWidth: 420
                            visible: id_root.targetStatusText.length > 0
                            text: id_root.targetStatusText
                            font.pixelSize: Themes.emulatorTarget.fontSizes.description
                            color: id_root.targetStatusIsError
                                ? Themes.emulatorTarget.colors.errorText
                                : Themes.emulatorTarget.colors.descriptionText
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            id: id_confirmTarget

                            readonly property bool canConfirm: id_root.selectedAppId > 0
                                && id_root.selectedName.trim().length > 0
                                && id_installLocationField.text.trim().length > 0
                                && id_prefixLocationField.text.trim().length > 0
                                && !id_root.isCreatingTarget

                            implicitHeight: 32
                            implicitWidth: id_confirmTargetLabel.implicitWidth + 60
                            radius: 6
                            enabled: canConfirm
                            opacity: enabled ? 1.0 : 0.45

                            color: id_confirmTargetMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.35)
                                : id_confirmTargetMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.26)
                                    : Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.10)

                            border.width: 1
                            border.color: id_confirmTargetMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.90)
                                : id_confirmTargetMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.75)
                                    : Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.62)

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Text {
                                id: id_confirmTargetLabel

                                anchors.centerIn: parent
                                text: qsTr("Confirm")
                                color: id_root.themedCompletionColor
                                font.pixelSize: Themes.emulatorTarget.fontSizes.confirmButton
                            }

                            MouseArea {
                                id: id_confirmTargetMouseArea
                                
                                anchors.fill: parent
                                enabled: id_confirmTarget.canConfirm
                                hoverEnabled: enabled
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                                onClicked: id_root.createTarget()
                            }
                        }
                    }
                }
            }
        }
    }
}
