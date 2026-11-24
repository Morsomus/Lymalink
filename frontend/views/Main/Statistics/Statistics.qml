/////////////////////////////////////////////////////////
// File: Statistics.qml
// Date: 2026-11-23
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Statistics
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Public ________________________________________________
    property bool p_active: false

    // Internals _____________________________________________
    property int activeTab: ctxSettings.statisticsActiveTab
    property var snapshot: ({})
    property string refreshError: ""
    property string activeSearch: ""
    property string activeSort: ctxSettings.statisticsToolbarSort
    property bool sortDescending: ctxSettings.statisticsToolbarSortDescending
    readonly property bool steam: activeTab === 1
    readonly property var source: snapshot[steam ? "steamImports" : "emulators"] || ({})
    readonly property var summary: source.summary || ({})
    readonly property int completionGameCount: summary.gamesWithAchievements || 0
    readonly property var games: source.games || []
    readonly property var filteredGames: filterGames()
    readonly property bool compactRows: id_games.width < 680
    readonly property color accentColor: Themes.globalStyle.progressColor(ctxSettings.globalColorStyle)
    readonly property color panelColor: Qt.tint(Themes.statistics.colors.panel,
        Themes.globalStyle.withAlpha(accentColor, Themes.statistics.opacity.panel))
    readonly property color panelBorderColor: Qt.tint(Themes.statistics.colors.border,
        Themes.globalStyle.withAlpha(accentColor, Themes.statistics.opacity.border))
    readonly property color alternateRowColor: Qt.tint(Themes.statistics.colors.alternateRow,
        Themes.globalStyle.withAlpha(accentColor, Themes.statistics.opacity.alternateRow))

    onP_activeChanged: {
        if (p_active) {
            refresh()
        } else {
            id_toolbar.closeOpenPanels()
        }
    }
    onFilteredGamesChanged: id_games.positionViewAtBeginning()
    onActiveTabChanged: id_games.positionViewAtBeginning()

    function refresh() {
        refreshError = ""
        ctxLymalink.RefreshStatistics(false)
    }

    function playtime(seconds) {
        const minutes = Math.floor(Math.max(0, Number(seconds || 0)) / 60)
        const hours = Math.floor(minutes / 60)
        const remainingMinutes = minutes % 60
        const playtimeText = qsTr("%1h %2m").arg(hours).arg(remainingMinutes)
        
        return playtimeText
    }

    function dateText(timestamp) {
        let formattedDate = "—"
        if (timestamp > 0) {
            const date = new Date(timestamp * 1000)
            formattedDate = Qt.formatDateTime(date, "yyyy-MM-dd")
        }

        return formattedDate
    }

    function completion(value) {
        let completionText = qsTr("—")
        if (value >= 0) {
            const percentage = Number(value).toLocaleString(Qt.locale(), "f", 1)
            completionText = percentage + "%"
        }

        return completionText
    }

    function filterGames() {
        const search = activeSearch.trim().toLocaleLowerCase()
        const result = games.filter(function(game) {
            const title = game.title.toLocaleLowerCase()
            const matchesSearch = title.indexOf(search) !== -1

            return matchesSearch
        })
        const field = {
            playtime: "playtimeSeconds",
            completion: "completion",
            lastPlayed: "lastPlayedTimestamp",
            latestUnlock: "latestUnlockTimestamp"
        }[activeSort]
        const dateSort = activeSort === "lastPlayed" || activeSort === "latestUnlock"

        result.sort(function(a, b) {
            const titleA = a.title.toLocaleLowerCase()
            const titleB = b.title.toLocaleLowerCase()
            const titleOrder = titleA.localeCompare(titleB)
            const appIdOrder = a.appId - b.appId
            if (activeSort === "title") {
                const directedTitleOrder = sortDescending ? -titleOrder : titleOrder
                const comparison = directedTitleOrder || appIdOrder

                return comparison
            }

            // Missing values stay last in either order
            const missingA = activeSort === "completion" ? !a.hasAchievementData : (dateSort && a[field] <= 0)
            const missingB = activeSort === "completion" ? !b.hasAchievementData : (dateSort && b[field] <= 0)
            if (missingA !== missingB) {
                const missingOrder = missingA ? 1 : -1

                return missingOrder
            }

            const difference = a[field] - b[field]
            const directedDifference = sortDescending ? -difference : difference
            const comparison = directedDifference || titleOrder || appIdOrder

            return comparison
        })

        return result
    }

    Connections {
        target: ctxSettings

        function onSignalDefaultsReset() {
            id_root.activeSort = ctxSettings.statisticsToolbarSort
            id_root.sortDescending = ctxSettings.statisticsToolbarSortDescending
            id_root.activeTab = ctxSettings.statisticsActiveTab
        }
    }

    Connections {
        target: ctxLymalink

        function onSignalStatisticsReady(payload) {
            if (payload.success) {
                id_root.snapshot = payload
                id_root.refreshError = ""
            } else {
                id_root.refreshError = payload.error || qsTr("Couldn't refresh statistics.")
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_Text: Label {
        id: id_text

        color: Themes.statistics.colors.body
        font.pixelSize: Themes.statistics.fontSizes.body
        elide: Text.ElideRight
        textFormat: Text.PlainText

        HoverHandler {
            id: id_textHover
        }

        CustomTooltip {
            p_active: id_textHover.hovered && id_text.truncated
            p_text: id_text.text
        }
    }

    component C_Divider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: id_root.panelBorderColor
    }

    component C_SummaryCard: Rectangle {
        id: id_card

        property string title: ""
        property string value: ""
        property string detail: ""

        implicitHeight: Math.max(104, id_summaryContent.implicitHeight + 28)
        color: id_root.panelColor
        border.color: id_root.panelBorderColor
        radius: 8

        ColumnLayout {
            id: id_summaryContent

            anchors.fill: parent
            anchors.margins: 14
            spacing: 4

            C_Text {
                Layout.fillWidth: true
                text: id_card.title
            }

            C_Divider {}

            C_Text {
                Layout.fillWidth: true
                text: id_card.value
                color: Themes.statistics.colors.title
                font.pixelSize: Themes.statistics.fontSizes.value
                font.bold: true
                fontSizeMode: Text.Fit
                minimumPixelSize: Themes.statistics.fontSizes.body
            }

            C_Text {
                Layout.fillWidth: true
                text: id_card.detail
                font.pixelSize: Themes.statistics.fontSizes.caption
            }
        }
    }

    component C_Highlights: Rectangle {
        id: id_highlights

        property string title: ""
        property var entries: []
        property bool rarity: false

        implicitHeight: id_highlightContent.implicitHeight + 28
        color: id_root.panelColor
        border.color: id_root.panelBorderColor
        radius: 8

        ColumnLayout {
            id: id_highlightContent

            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            C_Text {
                Layout.fillWidth: true
                text: id_highlights.title
                color: Themes.statistics.colors.title
                font.pixelSize: Themes.statistics.fontSizes.section
                font.bold: true
            }

            C_Divider {}

            C_Text {
                Layout.fillWidth: true
                visible: id_highlights.entries.length === 0
                text: id_highlights.rarity
                    ? qsTr("No global unlock percentages available for unlocked achievements.")
                    : qsTr("No dated achievement unlocks found.")
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: id_highlights.entries

                delegate: ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 2

                    C_Text {
                        Layout.fillWidth: true
                        text: modelData.achievementName
                        color: Themes.statistics.colors.title
                    }

                    C_Text {
                        Layout.fillWidth: true
                        text: modelData.title + " · " + (id_highlights.rarity
                            ? qsTr("%1% global").arg(Number(modelData.globalUnlockPercentage).toLocaleString(Qt.locale(), "f", 2))
                            : id_root.dateText(modelData.unlockTimestamp))
                        font.pixelSize: Themes.statistics.fontSizes.caption
                    }
                }
            }
        }
    }

    component C_Activity: ColumnLayout {
        id: id_activity

        property string title: ""
        property string value: ""
        property string detail: ""

        spacing: 5

        C_Text {
            Layout.fillWidth: true
            text: id_activity.title
            font.pixelSize: Themes.statistics.fontSizes.caption
        }

        C_Divider {
            visible: id_activityLayout.columns === 3
        }

        C_Text {
            Layout.fillWidth: true
            text: id_activity.value
            color: Themes.statistics.colors.title
            font.pixelSize: Themes.statistics.fontSizes.activityValue
            font.bold: true
        }

        C_Text {
            Layout.fillWidth: true
            text: id_activity.detail
            font.pixelSize: Themes.statistics.fontSizes.caption
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        anchors.topMargin: 48
        spacing: 16

        DashboardToolbar {
            id: id_toolbar

            Layout.fillWidth: true
            p_toolbarTitle: qsTr("Statistics")
            p_statisticsVisible: true
            p_statisticsBusy: ctxLymalink.statisticsBusy
            p_statisticsSort: id_root.activeSort
            p_statisticsDescending: id_root.sortDescending
            onStatisticsSearchChanged: function(text) {
                id_root.activeSearch = text
            }
            onStatisticsSortSelected: function(sort) {
                id_root.activeSort = sort
                ctxSettings.SaveValue(Settings.StatisticsToolbarSort, sort)
            }
            onStatisticsOrderSelected: function(descending) {
                id_root.sortDescending = descending
                ctxSettings.SaveValue(Settings.StatisticsToolbarSortDescending, descending)
            }
            onStatisticsGlobalPercentagesUpdateClicked: ctxLymalink.RefreshStatistics(true)
            onRefreshClicked: id_root.refresh()
        }

        CustomTabSelector {
            Layout.preferredWidth: Math.min(320, parent.width)
            p_firstText: qsTr("Emulators")
            p_secondText: qsTr("Steam Imports")
            p_currentIndex: id_root.activeTab
            p_accentColor: id_root.accentColor
            onActivated: function(index) {
                id_root.activeTab = index
                ctxSettings.SaveValue(Settings.StatisticsActiveTab, index)
            }
        }

        C_Text {
            Layout.fillWidth: true
            visible: id_root.refreshError.length > 0
            text: id_root.refreshError + (id_root.source.summary ? " " + qsTr("Showing last loaded statistics.") : "")
            color: Themes.statistics.colors.error
            wrapMode: Text.WordWrap
        }

        ListView {
            id: id_games

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: id_root.filteredGames
            boundsBehavior: Flickable.StopAtBounds
            onWidthChanged: positionViewAtBeginning()
            ScrollBar.vertical: CustomScrollBar {}

            header: ColumnLayout {
                width: id_games.width - 22
                spacing: 14
                onHeightChanged: {
                    Qt.callLater(function() {
                        id_games.positionViewAtBeginning()
                    })
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width < 720 ? 2 : 4
                    columnSpacing: 10
                    rowSpacing: 10

                    C_SummaryCard {
                        Layout.fillWidth: true
                        title: qsTr("Games")
                        value: String(id_root.completionGameCount)
                        detail: !id_root.steam && id_root.summary.hiddenGameCount > 0
                            ? qsTr("%1 total games (Includes hidden games)").arg(id_root.summary.gameCount || 0)
                            : qsTr("%1 total games").arg(id_root.summary.gameCount || 0)
                    }

                    C_SummaryCard {
                        Layout.fillWidth: true
                        title: qsTr("Playtime")
                        value: id_root.playtime(id_root.summary.playtimeSeconds)
                        detail: qsTr("Total")
                    }

                    C_SummaryCard {
                        Layout.fillWidth: true
                        title: qsTr("Achievements")
                        value: qsTr("%1 / %2").arg(id_root.summary.unlockedAchievements || 0).arg(id_root.summary.totalAchievements || 0)
                        detail: id_root.completion(id_root.summary.completion === undefined ? -1 : id_root.summary.completion)
                    }

                    C_SummaryCard {
                        Layout.fillWidth: true
                        title: qsTr("Completed games")
                        value: String(id_root.summary.completedGames || 0)
                        detail: id_root.completion(id_root.completionGameCount > 0
                            ? 100 * (id_root.summary.completedGames || 0) / id_root.completionGameCount
                            : 0)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: id_activityLayout.implicitHeight + 32
                    color: id_root.panelColor
                    border.color: id_root.panelBorderColor
                    radius: 8

                    GridLayout {
                        id: id_activityLayout

                        anchors.fill: parent
                        anchors.margins: 16
                        columns: width < 680 ? 1 : 3
                        columnSpacing: 24
                        rowSpacing: 16

                        C_Activity {
                            readonly property var game: id_root.summary.mostPlayed || ({})

                            Layout.fillWidth: true
                            title: qsTr("Most played")
                            value: game.title || qsTr("No saved playtime")
                            detail: game.title ? id_root.playtime(game.playtimeSeconds) : "—"
                        }

                        C_Activity {
                            readonly property var game: id_root.summary.recentlyPlayed || ({})

                            Layout.fillWidth: true
                            title: qsTr("Recently played")
                            value: game.title || qsTr("No played dates saved")
                            detail: id_root.dateText(game.lastPlayedTimestamp)
                        }

                        C_Activity {
                            readonly property var unlock: id_root.summary.latestUnlock || ({})

                            Layout.fillWidth: true
                            title: qsTr("Latest unlock")
                            value: unlock.achievementName || qsTr("No dated unlocks saved")
                            detail: unlock.title ? unlock.title + " · " + id_root.dateText(unlock.unlockTimestamp) : "—"
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: width < 720 ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    C_Highlights {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        title: qsTr("Recent unlocks")
                        entries: id_root.source.recentUnlocks || []
                    }

                    C_Highlights {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        title: qsTr("Rarest unlocked achievements")
                        entries: id_root.source.rarestUnlocks || []
                        rarity: true
                    }
                }

                C_Text {
                    Layout.fillWidth: true
                    text: qsTr("Games (%1)").arg(id_root.filteredGames.length)
                    color: Themes.statistics.colors.title
                    font.pixelSize: Themes.statistics.fontSizes.section
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    C_Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        text: qsTr("Title")
                    }

                    C_Text {
                        Layout.preferredWidth: 80
                        Layout.minimumWidth: 80
                        Layout.maximumWidth: 80
                        text: qsTr("Completion")
                    }

                    C_Text {
                        Layout.preferredWidth: 100
                        Layout.minimumWidth: 100
                        Layout.maximumWidth: 100
                        text: qsTr("Achievements")
                    }

                    C_Text {
                        Layout.preferredWidth: 75
                        text: qsTr("Playtime")
                    }

                    C_Text {
                        visible: !id_root.compactRows
                        Layout.preferredWidth: 90
                        text: qsTr("Last played")
                    }

                    C_Text {
                        visible: !id_root.compactRows
                        Layout.preferredWidth: 90
                        text: qsTr("Latest unlock")
                    }
                }

                C_Divider {}

                C_Text {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 12
                    visible: id_root.filteredGames.length === 0 && !ctxLymalink.statisticsBusy
                    text: id_root.games.length > 0 ? qsTr("No games match your search.")
                        : (id_root.steam ? qsTr("No Steam imports found.") : qsTr("No emulator games found."))
                }
            }

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: id_games.width - 22
                height: Math.max(id_root.compactRows ? 80 : 38, id_gameRow.implicitHeight + 8)
                color: index % 2 === 0 ? "transparent" : id_root.alternateRowColor
                radius: 4

                RowLayout {
                    id: id_gameRow

                    anchors.fill: parent
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.minimumWidth: 0
                        spacing: 3

                        C_Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Themes.statistics.colors.title
                        }

                        C_Text {
                            Layout.fillWidth: true
                            visible: modelData.hidden
                            text: qsTr("Hidden")
                            font.pixelSize: Themes.statistics.fontSizes.caption
                        }

                        C_Text {
                            Layout.fillWidth: true
                            visible: id_root.compactRows
                            text: qsTr("Latest unlock: %1").arg(id_root.dateText(modelData.latestUnlockTimestamp))
                            font.pixelSize: Themes.statistics.fontSizes.caption
                        }

                        C_Text {
                            Layout.fillWidth: true
                            visible: id_root.compactRows
                            text: qsTr("Last played: %1").arg(id_root.dateText(modelData.lastPlayedTimestamp))
                            font.pixelSize: Themes.statistics.fontSizes.caption
                        }
                    }

                    C_Text {
                        Layout.preferredWidth: 80
                        Layout.minimumWidth: 80
                        Layout.maximumWidth: 80
                        text: id_root.completion(modelData.completion)
                    }

                    C_Text {
                        Layout.preferredWidth: 100
                        Layout.minimumWidth: 100
                        Layout.maximumWidth: 100
                        text: modelData.hasAchievementData
                            ? qsTr("%1 / %2").arg(modelData.unlockedAchievements).arg(modelData.totalAchievements)
                            : "—"
                    }

                    C_Text {
                        Layout.preferredWidth: 75
                        text: id_root.playtime(modelData.playtimeSeconds)
                    }

                    C_Text {
                        visible: !id_root.compactRows
                        Layout.preferredWidth: 90
                        text: id_root.dateText(modelData.lastPlayedTimestamp)
                        font.pixelSize: Themes.statistics.fontSizes.caption
                    }

                    C_Text {
                        visible: !id_root.compactRows
                        Layout.preferredWidth: 90
                        text: id_root.dateText(modelData.latestUnlockTimestamp)
                        font.pixelSize: Themes.statistics.fontSizes.caption
                    }
                }
            }
        }
    }
}
