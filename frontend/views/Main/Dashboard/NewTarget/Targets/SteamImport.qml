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

    signal importsApplied(var loadingAppIds)

    // Internals _____________________________________________
    property bool libraryLoaded: false
    property bool passcodeUnlocked: false
    property bool libraryLoading: false
    property bool updateLoading: false
    property bool applyLoading: false
    property string pendingOperation: ""
    property string statusText: ""
    property bool statusIsError: false
    property string updateStatusText: ""
    property bool updateStatusIsError: false
    property string unlockedSteamWebApiKey: ""
    property bool awaitingUnlockAction: false
    property var libraryGames: []
    property real resultsScrollY: 0
    property var pendingUpdateGames: []
    property int updateProgressCurrent: 0
    property int updateProgressTotal: 0
    property var updateAggregatePayload: ({ updatedCount: 0, skippedCount: 0, assetRefreshAppIds: [], errors: [] })
    property var pendingImportGames: []
    property int importProgressCurrent: 0
    property int importProgressTotal: 0
    property int applyRemovalFailures: 0
    property var applyAggregatePayload: ({ importedCount: 0, skippedCount: 0, importedAppIds: [], errors: [] })

    // Derived properties for UI state and validation
    readonly property bool steamConfigured: ctxSettings.steamId.trim().length > 0 && ctxSettings.steamWebApiKey !== ""
    readonly property var visibleLibraryGames: filterLibraryGames()
    readonly property var newImports: changedGames(false, true)
    readonly property var removals: changedGames(true, false)
    readonly property int selectedCount: countSelectedGames()
    readonly property bool selectionDirty: newImports.length > 0 || removals.length > 0
    readonly property bool operationLoading: libraryLoading || updateLoading || applyLoading
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)

    // Filters the loaded steam library based on search field text (name or appId)
    function filterLibraryGames() {
        const term = id_searchField.text.trim().toLocaleLowerCase()
        const filteredGames = term.length === 0
            ? id_root.libraryGames
            : id_root.libraryGames.filter(function(game) {
                return game.name.toLocaleLowerCase().includes(term) || game.appId.toString().includes(term)
            })

        return filteredGames.slice().sort(function(left, right) {
            const nameCompare = left.name.localeCompare(right.name, undefined, { sensitivity: "base" })
            if (nameCompare !== 0) {
                return nameCompare
            }

            return left.appId - right.appId
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
            id_root.updateStatusText = qsTr("Please unlock your Steam Web API key to update Steam imports.")
            id_root.updateStatusIsError = true
            return
        }

        id_root.updateLoading = true
        id_root.updateStatusText = qsTr("Checking imported games...")
        id_root.updateStatusIsError = false

        const payload = ctxLymalink.FetchSteamOwnedGames(ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        id_root.updateLoading = false

        if (!payload.success) {
            id_root.updateStatusText = payload.error.length > 0
                ? payload.error
                : qsTr("Imported games could not be checked.")
            id_root.updateStatusIsError = true
            return
        }

        id_root.libraryGames = payload.games
        id_root.libraryLoaded = true

        const importedGames = id_root.libraryGames.filter(function(game) {
            return game.imported
        })

        if (importedGames.length === 0) {
            id_root.updateStatusText = qsTr("No imported games found to update.")
            id_root.updateStatusIsError = false
            return
        }

        id_root.pendingUpdateGames = importedGames
        id_root.updateProgressCurrent = 0
        id_root.updateProgressTotal = importedGames.length
        id_root.updateAggregatePayload = {
            updatedCount: 0,
            skippedCount: 0,
            assetRefreshAppIds: [],
            errors: []
        }
        id_root.updateLoading = true
        id_root.updateStatusText = qsTr("Updating progress %1/%2...").arg(0).arg(importedGames.length)
        id_root.updateStatusIsError = false
        id_updateImportTimer.restart()
    }

    // Update one imported game per timer tick so progress text can repaint between blocking backend calls
    function executeNextUpdateImport() {
        if (id_root.updateProgressCurrent >= id_root.updateProgressTotal) {
            id_root.finishUpdateSteamImports()
            return
        }

        const game = id_root.pendingUpdateGames[id_root.updateProgressCurrent]
        const updatePayload = ctxLymalink.UpdateSteamImports([game], ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        const aggregate = id_root.updateAggregatePayload

        aggregate.updatedCount += updatePayload.updatedCount ?? 0
        aggregate.skippedCount += updatePayload.skippedCount ?? 0
        aggregate.assetRefreshAppIds = aggregate.assetRefreshAppIds.concat(updatePayload.assetRefreshAppIds ?? [])
        aggregate.errors = aggregate.errors.concat(updatePayload.errors ?? [])
        id_root.updateAggregatePayload = aggregate

        id_root.updateProgressCurrent += 1
        id_root.updateStatusText = qsTr("Updating progress %1/%2...").arg(id_root.updateProgressCurrent).arg(id_root.updateProgressTotal)

        if (id_root.shouldStopSteamBatch(updatePayload)) {
            id_root.updateProgressTotal = id_root.updateProgressCurrent
        }

        id_updateImportTimer.restart()
    }

    // Finalize accumulated update results and mark targets that need a fresh asset/metadata pass
    function finishUpdateSteamImports() {
        id_root.updateLoading = false

        const updatePayload = id_root.updateAggregatePayload
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

        id_root.updateStatusText = status
        id_root.updateStatusIsError = updatedCount === 0 && errors.length > 0
        id_root.statusText = status
        id_root.statusIsError = id_root.updateStatusIsError
        id_root.importsApplied(assetRefreshAppIds)
    }

    // Privacy failures apply to the whole Steam account, so remaining per-game calls would repeat the same error
    function shouldStopSteamBatch(payload) {
        const errors = payload.errors ?? []
        for (let i = 0; i < errors.length; ++i) {
            if (String(errors[i]).indexOf("Steam profile is not public") !== -1) {
                return true
            }
        }

        return false
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
    function setGameSelected(appId, selected, preserveScroll) {
        if (preserveScroll) {
            id_root.resultsScrollY = id_resultsList.contentY
        }

        id_root.libraryGames = id_root.libraryGames.map(function(game) {
            return game.appId === appId
                ? Object.assign({}, game, { selected: selected })
                : game
        })

        if (preserveScroll) {
            Qt.callLater(function() {
                id_resultsList.contentY = id_root.resultsScrollY
            })
        }

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
            id_root.prepareApplySelection()
        }
    }

    // Start apply flow on the next event-loop pass so the loading state is visible before work begins
    function prepareApplySelection() {
        const imports = id_root.newImports
        const removals = id_root.removals

        if (imports.length === 0 && removals.length === 0) {
            id_root.statusText = qsTr("No Steam import changes to apply.")
            id_root.statusIsError = false
            return
        }

        const importWord = imports.length === 1 ? "game" : "games"
        id_root.applyLoading = true
        id_root.statusText = imports.length > 0
            ? qsTr("Importing %1 %2 %3/%4...").arg(imports.length).arg(importWord).arg(0).arg(imports.length)
            : qsTr("Applying Steam import changes...")
        id_root.statusIsError = false
        id_applySelectionTimer.restart()
    }

    // Apply removals synchronously first; imports are chunked afterward because they perform Steam API calls
    function executeApplySelection() {
        const imports = id_root.newImports
        const removals = id_root.removals
        id_root.applyRemovalFailures = 0

        // Process removals first (delete from backend/local state)
        if (removals.length > 0) {
            for (let i = 0; i < removals.length; ++i) {
                const removal = removals[i]
                if (!ctxLymalink.DeleteTarget(removal.appId, "Steam")) {
                    ++id_root.applyRemovalFailures
                }
            }
        }

        id_root.pendingImportGames = imports
        id_root.importProgressCurrent = 0
        id_root.importProgressTotal = imports.length
        id_root.applyAggregatePayload = {
            importedCount: 0,
            skippedCount: 0,
            importedAppIds: [],
            errors: []
        }

        if (imports.length === 0) {
            id_root.finishApplySelection()
            return
        }

        id_root.statusText = qsTr("Importing %1/%2...").arg(0).arg(imports.length)
        id_importSelectionTimer.restart()
    }

    // Import one selected game per tick to keep the UI responsive and status counter current
    function executeNextImportSelection() {
        if (id_root.importProgressCurrent >= id_root.importProgressTotal) {
            id_root.finishApplySelection()
            return
        }

        const game = id_root.pendingImportGames[id_root.importProgressCurrent]
        const payload = ctxLymalink.ImportSteamGames([game], ctxSettings.steamId, id_root.unlockedSteamWebApiKey)
        const aggregate = id_root.applyAggregatePayload

        aggregate.importedCount += payload.importedCount ?? 0
        aggregate.skippedCount += payload.skippedCount ?? 0
        aggregate.importedAppIds = aggregate.importedAppIds.concat(payload.importedAppIds ?? [])
        aggregate.errors = aggregate.errors.concat(payload.errors ?? [])
        id_root.applyAggregatePayload = aggregate

        id_root.importProgressCurrent += 1
        id_root.statusText = qsTr("Importing %1/%2...").arg(id_root.importProgressCurrent).arg(id_root.importProgressTotal)

        if (id_root.shouldStopSteamBatch(payload)) {
            id_root.importProgressTotal = id_root.importProgressCurrent
        }

        id_importSelectionTimer.restart()
    }

    function finishApplySelection() {
        const imports = id_root.pendingImportGames
        const removals = id_root.removals
        const payload = id_root.applyAggregatePayload
        const importedAppIds = payload.importedAppIds ?? []

        // Queue background tasks to fetch assets/metadata for newly imported games
        for (let i = 0; i < importedAppIds.length; ++i) {
            ctxLymalink.EnqueueSteamHydrationTask(importedAppIds[i], true, "Steam")
        }

        const importedCount = payload.importedCount ?? 0
        const importedWord = importedCount === 1 ? "game" : "games"
        let status = ""

        if (removals.length > 0) {
            const removalWord = removals.length === 1 ? "game" : "games"
            status = qsTr("Removed %1 Steam %2.").arg(removals.length - id_root.applyRemovalFailures).arg(removalWord)
            if (id_root.applyRemovalFailures > 0) {
                status += " " + qsTr("%1 removal(s) failed.").arg(id_root.applyRemovalFailures)
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
        if (id_root.applyRemovalFailures > 0) {
            errors.push(qsTr("%1 removal(s) failed.").arg(id_root.applyRemovalFailures))
        }
        if (errors.length > 0) {
            status += "\n" + errors.slice(0, 3).join("\n") // Limit to first 3 errors for readability
        }

        id_root.loadSteamLibrary()
        id_root.statusText = status
        id_root.statusIsError = (importedCount === 0 && errors.length > 0) || id_root.applyRemovalFailures > 0
        id_root.applyLoading = false
        id_root.importsApplied(payload.importedAppIds ?? [])
    }

    Timer {
        id: id_updateImportTimer

        interval: 50
        repeat: false
        onTriggered: id_root.executeNextUpdateImport()
    }

    Timer {
        id: id_applySelectionTimer

        interval: 50
        repeat: false
        onTriggered: id_root.executeApplySelection()
    }

    Timer {
        id: id_importSelectionTimer

        interval: 50
        repeat: false
        onTriggered: id_root.executeNextImportSelection()
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
        p_shortcutEnabled: true
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
    Item {
        id: id_scrollView

        readonly property real availableWidth: width
        readonly property real availableHeight: height

        anchors.fill: parent
        clip: true

        Flickable {
            id: id_parentFlickable

            width: id_scrollView.availableWidth
            height: id_scrollView.availableHeight
            contentWidth: width
            contentHeight: id_content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: !id_resultsListHover.hovered

            ScrollBar.vertical: CustomScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                id: id_content

                readonly property int sideMargin: 20

                x: Math.max(sideMargin, (id_parentFlickable.width - width) / 2)
                width: Math.max(0, Math.min(id_parentFlickable.width - sideMargin * 2, 760))
                spacing: 16

                // Info box
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                    implicitHeight: id_introText.implicitHeight + id_publicProfileText.implicitHeight + 38
                    radius: 6
                    color: Themes.steamImportTarget.colors.infoBlockBackground
                    border.width: 1
                    border.color: Themes.steamImportTarget.colors.infoBlockBorder

                    Text {
                        id: id_introText

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.topMargin: 14
                        text: qsTr("Steam import requires your Steam ID and an encrypted Web API key configured under Settings.")
                        wrapMode: Text.WordWrap
                        color: Themes.steamImportTarget.colors.descriptionText
                        font.pixelSize: Themes.steamImportTarget.fontSizes.description
                    }

                    Text {
                        id: id_publicProfileText

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: id_introText.bottom
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.topMargin: 8
                        text: qsTr("Please ensure your Steam profile is set to Public for the import process to succeed.")
                        wrapMode: Text.WordWrap
                        color: Themes.steamImportTarget.colors.warningText
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

                    CustomButton {
                        text: qsTr("Update")
                        enabled: id_root.steamConfigured && !id_root.operationLoading
                        onClicked: id_root.beginOperation("update")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: id_root.updateStatusText.length > 0 || id_root.updateLoading

                        CustomBusyIndicator {
                            Layout.alignment: Qt.AlignVCenter
                            p_indicatorSize: 24
                            p_speed: 900
                            p_running: id_root.updateLoading
                        }

                        Text {
                            Layout.fillWidth: true
                            text: id_root.updateStatusText
                            wrapMode: Text.WordWrap
                            color: id_root.updateStatusIsError
                                ? Themes.steamImportTarget.colors.errorText
                                : id_root.themedCompletionColor
                            font.pixelSize: Themes.steamImportTarget.fontSizes.description
                        }
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
                        visible: id_root.libraryLoaded && !id_root.updateLoading
                        spacing: 8

                        CustomTextField {
                            id: id_searchField

                            Layout.fillWidth: true
                            visible: id_root.libraryLoaded
                            enabled: !id_root.applyLoading
                            placeholderText: qsTr("Search game name or App ID")
                            font.pixelSize: Themes.steamImportTarget.fontSizes.input
                        }

                        CustomButton {
                            text: qsTr("Select All")
                            enabled: !id_root.applyLoading && id_root.selectedCount < id_root.libraryGames.length
                            onClicked: id_root.setAllGamesSelected(true)
                        }

                        CustomButton {
                            text: qsTr("Deselect All")
                            enabled: !id_root.applyLoading && id_root.selectedCount > 0
                            onClicked: id_root.setAllGamesSelected(false)
                        }
                    }

                    // Changes information
                    RowLayout {
                        Layout.fillWidth: true
                        visible: id_root.libraryLoaded && !id_root.updateLoading
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

                    Text {
                        visible: id_root.newImports.length > 10
                        Layout.fillWidth: true
                        text: qsTr("Large Steam import selected. We recommend smaller batches because asset loading may take a long time.")
                        wrapMode: Text.WordWrap
                        color: Themes.steamImportTarget.colors.errorText
                        font.pixelSize: Themes.steamImportTarget.fontSizes.description
                    }

                    // Game results
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: id_root.libraryLoaded && !id_root.updateLoading
                        Layout.preferredHeight: 270

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 6
                            color: Themes.steamImportTarget.colors.resultBackground
                            border.width: 1
                            border.color: Themes.steamImportTarget.colors.resultBorder

                            ListView {
                                id: id_resultsList

                                readonly property int rowScrollbarReserve: id_root.visibleLibraryGames.length >= 5 ? 25 : 0

                                anchors.fill: parent
                                anchors.margins: 6
                                clip: true
                                model: id_root.visibleLibraryGames
                                spacing: 8
                                boundsBehavior: Flickable.StopAtBounds

                                HoverHandler {
                                    id: id_resultsListHover
                                }

                                ScrollBar.vertical: CustomScrollBar {
                                    policy: id_root.visibleLibraryGames.length >= 5 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                                }

                                delegate: Rectangle {
                                    id: id_resultRow

                                    required property var modelData

                                    width: id_resultsList.width - id_resultsList.rowScrollbarReserve
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

                                        CustomCheckBox {
                                            id: id_gameCheckBox

                                            checked: id_resultRow.modelData.selected
                                            enabled: !id_root.applyLoading
                                            onClicked: id_root.setGameSelected(id_resultRow.modelData.appId, checked, true)
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
                                        enabled: !id_root.applyLoading
                                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: id_root.setGameSelected(id_resultRow.modelData.appId, !id_resultRow.modelData.selected, true)
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

                        CustomButton {
                            text: id_root.libraryLoaded ? qsTr("Reload Steam Library") : qsTr("Load Steam Library")
                            enabled: id_root.steamConfigured && !id_root.operationLoading
                            onClicked: id_root.beginOperation("load")
                        }

                        CustomBusyIndicator {
                            Layout.alignment: Qt.AlignVCenter
                            p_indicatorSize: 28
                            p_speed: 900
                            p_running: id_root.libraryLoading || id_root.applyLoading
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CustomButton {
                            text: qsTr("Apply Selection")
                            enabled: id_root.steamConfigured && id_root.selectionDirty && !id_root.operationLoading && id_root.libraryLoaded
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
}
