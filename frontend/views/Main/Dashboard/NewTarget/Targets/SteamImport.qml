/////////////////////////////////////////////////////////
// File: SteamImport.qml
// Date: 2026-06-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Import and update Steam targets
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    signal importsApplied()

    // Internals _____________________________________________
    property bool libraryLoaded: false
    property bool passcodeUnlocked: false
    property bool libraryLoading: false
    property string pendingOperation: ""
    property string statusText: ""
    property bool statusIsError: false
    property string unlockedSteamWebApiKey: ""
    property bool awaitingUnlockAction: false
    property var libraryGames: []

    // Derived properties for UI state and validation
    readonly property bool steamConfigured: ctxSettings.steamId.trim().length > 0 && ctxSettings.steamWebApiKey !== ""
    readonly property var visibleLibraryGames: filterLibraryGames()
    readonly property var newImports: changedGames(false, true)
    readonly property var removals: changedGames(true, false)
    readonly property int selectedCount: countSelectedGames()
    readonly property bool selectionDirty: newImports.length > 0 || removals.length > 0
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)

    // Filters the loaded steam library based on search field text (name or appId)
    function filterLibraryGames() {
        const term = id_searchField.text.trim().toLocaleLowerCase()
        if (term.length === 0) {
            return id_root.libraryGames
        }

        return id_root.libraryGames.filter(function(game) {
            return game.name.toLocaleLowerCase().includes(term) || game.appId.toString().includes(term)
        })
    }

    // Fetches the user's owned games from Steam using the decrypted Web API key
    function loadSteamLibrary() {
        if (!id_root.steamConfigured || id_root.unlockedSteamWebApiKey.length === 0) {
            id_root.statusText = qsTr("Please unlock your Steam Web API key to load your library.")
            id_root.statusIsError = true
            return
        }

        id_root.libraryLoading = true
        id_root.statusText = qsTr("Loading games...")
        id_root.statusIsError = false

        const payload = ctxLymalink.FetchSteamOwnedGames(ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        id_root.libraryLoading = false

        if (!payload.success) {
            id_root.libraryLoaded = false
            id_root.libraryGames = []
            id_root.statusText = payload.error.length > 0
                ? payload.error
                : qsTr("Games could not be loaded.")
            id_root.statusIsError = true
            return
        }

        id_root.libraryGames = payload.games
        id_root.libraryLoaded = true
        id_root.statusText = qsTr("Successfully loaded %1 Steam games.").arg(id_root.libraryGames.length)
        id_root.statusIsError = false
    }

    // Refreshes assets and achievement data for already imported Steam games
    function updateSteamImports() {
        if (!id_root.steamConfigured || id_root.unlockedSteamWebApiKey.length === 0) {
            id_root.statusText = qsTr("Please unlock your Steam Web API key to update Steam imports.")
            id_root.statusIsError = true
            return
        }

        id_root.libraryLoading = true
        id_root.statusText = qsTr("Checking imported games...")
        id_root.statusIsError = false

        const payload = ctxLymalink.FetchSteamOwnedGames(ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        id_root.libraryLoading = false

        if (!payload.success) {
            id_root.statusText = payload.error.length > 0
                ? payload.error
                : qsTr("Imported games could not be checked.")
            id_root.statusIsError = true
            return
        }

        id_root.libraryGames = payload.games
        id_root.libraryLoaded = true

        const importedGames = id_root.libraryGames.filter(function(game) {
            return game.imported
        })

        if (importedGames.length === 0) {
            id_root.statusText = qsTr("No imported games found to update.")
            id_root.statusIsError = false
            return
        }

        id_root.libraryLoading = true
        id_root.statusText = qsTr("Updating progress...")

        const updatePayload = ctxLymalink.UpdateSteamImports(importedGames, ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        id_root.libraryLoading = false

        const assetRefreshAppIds = updatePayload.assetRefreshAppIds ?? []
        for (let i = 0; i < assetRefreshAppIds.length; ++i) {
            ctxLymalink.EnqueueSteamHydrationTask(assetRefreshAppIds[i], true, "Steam")
        }

        const updatedCount = updatePayload.updatedCount ?? 0
        const skippedCount = updatePayload.skippedCount ?? 0
        const updatedWord = updatedCount === 1 ? "game" : "games"
        const refreshWord = assetRefreshAppIds.length === 1 ? "game" : "games"
        let status = qsTr("Updated progress for %1 imported %2.").arg(updatedCount).arg(updatedWord)

        if (assetRefreshAppIds.length > 0) {
            status += " " + qsTr("Queued asset refresh for %1 %2 with achievement changes.").arg(assetRefreshAppIds.length).arg(refreshWord)
        }
        if (skippedCount > 0) {
            const skippedWord = skippedCount === 1 ? "game" : "games"
            status += " " + qsTr("%1 %2 skipped or failed.").arg(skippedCount).arg(skippedWord)
        }

        const errors = updatePayload.errors ?? []
        if (errors.length > 0) {
            status += "\n" + errors.slice(0, 3).join("\n")
        }

        id_root.statusText = status
        id_root.statusIsError = updatedCount === 0 && errors.length > 0
        id_root.importsApplied()
    }

    // Returns a list of games that are either marked for import or removal
    function changedGames(imported, selected) {
        return id_root.libraryGames.filter(function(game) {
            return game.imported === imported && game.selected === selected
        })
    }

    // Calculates the total number of games currently selected for import
    function countSelectedGames() {
        return id_root.libraryGames.filter(function(game) {
            return game.selected
        }).length
    }

    // Updates the selection state for a specific game by appId
    function setGameSelected(appId, selected) {
        id_root.libraryGames = id_root.libraryGames.map(function(game) {
            return game.appId === appId
                ? Object.assign({}, game, { selected: selected })
                : game
        })
        id_root.statusText = ""
        id_root.statusIsError = false
    }

    // Sets the selection state for all games in the library
    function setAllGamesSelected(selected) {
        id_root.libraryGames = id_root.libraryGames.map(function(game) {
            return Object.assign({}, game, { selected: selected })
        })
        id_root.statusText = ""
        id_root.statusIsError = false
    }

    // Initiates a process (load/update/apply) and ensures passcode is unlocked first
    function beginOperation(operation) {
        if (!id_root.steamConfigured) {
            return
        }

        id_root.pendingOperation = operation
        if (id_root.passcodeUnlocked) {
            id_root.finishPreviewOperation()
        } else {
            id_passcodePopup.open()
        }
    }

    // Validates selection status and handles removal confirmation if necessary
    function requestApplySelection() {
        if (!id_root.selectionDirty) {
            return
        }

        id_root.beginOperation("apply")
    }

    // Executes the actual operation once authorization is confirmed
    function finishPreviewOperation() {
        const operation = id_root.pendingOperation
        id_root.pendingOperation = ""
        id_root.statusIsError = false

        // Route to the appropriate handler based on the pending operation type
        if (operation === "load") {
            id_root.loadSteamLibrary()
        } else if (operation === "update") {
            id_root.updateSteamImports()
        } else if (operation === "apply") {
            // Extract pending import and removal lists from the UI state
            const imports = id_root.newImports
            const removals = id_root.removals
            let removalFailures = 0

            if (imports.length === 0 && removals.length === 0) {
                id_root.statusText = qsTr("No Steam import changes to apply.")
                id_root.statusIsError = false
                return
            }

            // Process removals first (delete from backend/local state)
            if (removals.length > 0) {
                for (let i = 0; i < removals.length; ++i) {
                    const removal = removals[i]
                    if (!ctxLymalink.DeleteTarget(removal.appId, "Steam")) {
                        ++removalFailures
                    }
                }
            }
            let payload = {
                importedCount: 0,
                skippedCount: 0,
                importedAppIds: [],
                errors: []
            }

            // Process imports (call backend API and handle results)
            if (imports.length > 0) {
                const importWord = imports.length === 1 ? "game" : "games"
                id_root.statusText = qsTr("Importing %1 %2...").arg(imports.length).arg(importWord)
                id_root.statusIsError = false

                // Execute backend import operation
                payload = ctxLymalink.ImportSteamGames(imports, ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
                const importedAppIds = payload.importedAppIds ?? []

                // Queue background tasks to fetch assets/metadata for newly imported games
                for (let i = 0; i < importedAppIds.length; ++i) {
                    ctxLymalink.EnqueueSteamHydrationTask(importedAppIds[i], true, "Steam")
                }
            }

            const importedCount = payload.importedCount ?? 0
            const importedWord = importedCount === 1 ? "game" : "games"
            let status = ""

            if (removals.length > 0) {
                const removalWord = removals.length === 1 ? "game" : "games"
                status = qsTr("Removed %1 Steam %2.").arg(removals.length - removalFailures).arg(removalWord)
                if (removalFailures > 0) {
                    status += " " + qsTr("%1 removal(s) failed.").arg(removalFailures)
                }
            }

            if (imports.length > 0) {
                status += (status.length > 0 ? " " : "") + qsTr("Imported %1 Steam %2.").arg(importedCount).arg(importedWord)
            }

            if ((payload.skippedCount ?? 0) > 0) {
                const skippedCount = payload.skippedCount ?? 0
                const skippedWord = skippedCount === 1 ? "game" : "games"
                status += " " + qsTr("%1 %2 skipped or failed.").arg(skippedCount).arg(skippedWord)
            }
            
            const errors = payload.errors ?? []
            if (removalFailures > 0) {
                errors.push(qsTr("%1 removal(s) failed.").arg(removalFailures))
            }
            if (errors.length > 0) {
                status += "\n" + errors.slice(0, 3).join("\n") // Limit to first 3 errors for readability
            }

            id_root.statusText = status
            id_root.statusIsError = (importedCount === 0 && errors.length > 0) || removalFailures > 0

            // Signal that imports have been applied and refresh the library view
            id_root.importsApplied()
            id_root.loadSteamLibrary()
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Unlock Steam Web API Key
    ConfirmationPopup {
        id: id_passcodePopup

        p_title: qsTr("Unlock Steam Web API Key")
        p_description: qsTr("Enter passcode for saved Steam Web API key.")
        p_confirmText: qsTr("Continue")
        p_singleVerificationMode: true
        onClosed: {
            if (id_root.awaitingUnlockAction) {
                id_root.awaitingUnlockAction = false
                Qt.callLater(id_root.finishPreviewOperation)
                return
            }

            id_root.pendingOperation = ""
        }
        onConfirmed: function(passcode) {
            ctxSettings.SetTempEncryptionKey(passcode)
            const unlockedKey = ctxSettings.GetSteamWebApiKeyPlain()
            ctxSettings.SetTempEncryptionKey("")

            if (unlockedKey.length === 0) {
                id_root.passcodeUnlocked = false
                id_root.unlockedSteamWebApiKey = ""
                id_root.statusText = qsTr("Incorrect passcode. API key unlock failed.")
                id_root.statusIsError = true
                return
            }

            id_root.passcodeUnlocked = true
            id_root.unlockedSteamWebApiKey = unlockedKey
            id_root.awaitingUnlockAction = true
            id_passcodePopup.close()
        }
    }

    // Imports view
    ScrollView {
        id: id_scrollView

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            readonly property int sideMargin: 20

            x: Math.max(sideMargin, (id_scrollView.availableWidth - width) / 2)
            width: Math.max(0, Math.min(id_scrollView.availableWidth - sideMargin * 2, 760))
            spacing: 16

            // Info box
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 20
                implicitHeight: id_introText.implicitHeight + 28
                radius: 6
                color: Themes.steamImportTarget.colors.infoBlockBackground
                border.width: 1
                border.color: Themes.steamImportTarget.colors.infoBlockBorder

                Text {
                    id: id_introText

                    anchors.fill: parent
                    anchors.margins: 14
                    text: qsTr("Steam import requires your Steam ID and an encrypted Web API key configured under Settings.")
                    wrapMode: Text.WordWrap
                    color: Themes.steamImportTarget.colors.descriptionText
                    font.pixelSize: Themes.steamImportTarget.fontSizes.description
                }
            }

            // Warning text for missing API credentials
            Text {
                visible: !id_root.steamConfigured
                Layout.fillWidth: true
                text: qsTr("Please configure both Steam ID and Web API key in Settings to use Steam functions.")
                wrapMode: Text.WordWrap
                color: Themes.steamImportTarget.colors.errorText
                font.pixelSize: Themes.steamImportTarget.fontSizes.label
            }

            // Update Imports
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: qsTr("Update")
                    font.pixelSize: Themes.steamImportTarget.fontSizes.title
                    font.bold: true
                    color: Themes.steamImportTarget.colors.titleText
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Refresh data and achievements for already imported games.")
                    wrapMode: Text.WordWrap
                    color: Themes.steamImportTarget.colors.descriptionText
                    font.pixelSize: Themes.steamImportTarget.fontSizes.label
                }

                Button {
                    text: qsTr("Update")
                    enabled: id_root.steamConfigured && !id_root.libraryLoading
                    onClicked: id_root.beginOperation("update")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Themes.steamImportTarget.colors.divider
            }

            // Modify Imports (Add, Remove)
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: qsTr("Import / Remove")
                    font.pixelSize: Themes.steamImportTarget.fontSizes.title
                    font.bold: true
                    color: Themes.steamImportTarget.colors.titleText
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Load your library, then select games to import or uncheck to remove.")
                    wrapMode: Text.WordWrap
                    color: Themes.steamImportTarget.colors.descriptionText
                    font.pixelSize: Themes.steamImportTarget.fontSizes.label
                }

                // Select buttons
                RowLayout {
                    visible: id_root.libraryLoaded
                    spacing: 8

                    TextField {
                        id: id_searchField

                        Layout.fillWidth: true
                        visible: id_root.libraryLoaded
                        placeholderText: qsTr("Search game name or App ID")
                        font.pixelSize: Themes.steamImportTarget.fontSizes.input
                    }

                    Button {
                        text: qsTr("Select All")
                        enabled: id_root.selectedCount < id_root.libraryGames.length
                        onClicked: id_root.setAllGamesSelected(true)
                    }

                    Button {
                        text: qsTr("Deselect All")
                        enabled: id_root.selectedCount > 0
                        onClicked: id_root.setAllGamesSelected(false)
                    }
                }

                // Changes information
                RowLayout {
                    Layout.fillWidth: true
                    visible: id_root.libraryLoaded
                    spacing: 12

                    Text {
                        visible: id_root.libraryLoaded
                        Layout.fillWidth: true
                        text: qsTr("%1 selected, %2 new import(s), %3 removal(s)")
                            .arg(id_root.selectedCount)
                            .arg(id_root.newImports.length)
                            .arg(id_root.removals.length)
                        color: Themes.steamImportTarget.colors.descriptionText
                        font.pixelSize: Themes.steamImportTarget.fontSizes.description
                    }

                    Text {
                        text: id_root.selectionDirty
                            ? qsTr("Changes detected. Apply selection to confirm imports and removals.")
                            : qsTr("Selection matches imported games.")
                        wrapMode: Text.WordWrap
                        color: Themes.steamImportTarget.colors.descriptionText
                        font.pixelSize: Themes.steamImportTarget.fontSizes.description
                    }
                }

                // Game results
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: id_root.libraryLoaded
                    Layout.preferredHeight: 300

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 6
                        color: Themes.steamImportTarget.colors.resultBackground
                        border.width: 1
                        border.color: Themes.steamImportTarget.colors.resultBorder

                        ListView {
                            id: id_resultsList

                            anchors.fill: parent
                            anchors.margins: 6
                            clip: true
                            model: id_root.visibleLibraryGames
                            spacing: 8
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                id: id_resultRow

                                required property var modelData

                                width: id_resultsList.width
                                height: 48
                                radius: 6
                                color: id_resultMouseArea.containsMouse
                                    ? Themes.steamImportTarget.colors.resultBackgroundHover
                                    : Themes.steamImportTarget.colors.resultBackground
                                border.width: 1
                                border.color: Themes.steamImportTarget.colors.resultBorder

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    CheckBox {
                                        id: id_gameCheckBox

                                        checked: id_resultRow.modelData.selected
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            Layout.fillWidth: true
                                            text: id_resultRow.modelData.name
                                            elide: Text.ElideRight
                                            color: Themes.steamImportTarget.colors.labelText
                                            font.pixelSize: Themes.steamImportTarget.fontSizes.label
                                        }

                                        Text {
                                            text: qsTr("App ID: %1").arg(id_resultRow.modelData.appId)
                                            color: Themes.steamImportTarget.colors.descriptionText
                                            font.pixelSize: Themes.steamImportTarget.fontSizes.descriptionSubtle
                                        }
                                    }

                                    Rectangle {
                                        visible: id_resultRow.modelData.imported
                                        implicitWidth: id_importedText.implicitWidth + 14
                                        implicitHeight: 22
                                        radius: 5
                                        color: Themes.steamImportTarget.colors.importedBadgeBackground
                                        border.width: 1
                                        border.color: Themes.steamImportTarget.colors.importedBadgeBorder

                                        Text {
                                            id: id_importedText

                                            anchors.centerIn: parent
                                            text: qsTr("Imported")
                                            color: Themes.steamImportTarget.colors.importedBadgeText
                                            font.pixelSize: Themes.steamImportTarget.fontSizes.badge
                                        }
                                    }
                                }

                                MouseArea {
                                    id: id_resultMouseArea

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: id_root.setGameSelected(id_resultRow.modelData.appId, !id_resultRow.modelData.selected)
                                }
                            }
                        }
                    }

                    Text {
                        visible: id_root.visibleLibraryGames.length === 0
                        Layout.fillWidth: true
                        text: qsTr("No matching games")
                        color: Themes.steamImportTarget.colors.descriptionText
                        font.pixelSize: Themes.steamImportTarget.fontSizes.label
                    }
                }

                // Buttons row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        text: id_root.libraryLoaded ? qsTr("Reload Steam Library") : qsTr("Load Steam Library")
                        enabled: id_root.steamConfigured && !id_root.libraryLoading
                        onClicked: id_root.beginOperation("load")
                    }

                    CustomBusyIndicator {
                        Layout.alignment: Qt.AlignVCenter
                        p_indicatorSize: 28
                        p_speed: 900
                        p_running: id_root.libraryLoading
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Apply Selection")
                        enabled: id_root.steamConfigured && id_root.selectionDirty && !id_root.libraryLoading && id_root.libraryLoaded
                        onClicked: id_root.requestApplySelection()
                    }
                }
            }

            // Status text
            Text {
                visible: id_root.statusText.length > 0
                Layout.fillWidth: true
                Layout.bottomMargin: 20
                text: id_root.statusText
                wrapMode: Text.WordWrap
                color: id_root.statusIsError
                    ? Themes.steamImportTarget.colors.errorText
                    : id_root.themedCompletionColor
                font.pixelSize: Themes.steamImportTarget.fontSizes.label
            }
        }
    }
}
