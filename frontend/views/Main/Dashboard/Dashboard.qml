/////////////////////////////////////////////////////////
// File: Dashboard.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Dashboard displaying tracked content.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Internals _____________________________________________
    property string activeLayout: ctxSettings.dashboardToolbarLayout // list, detailedList, smallCardGrid, defaultCardGrid
    property bool noTargetsAvailable: false
    property var pendingTargetDetails: null
    property bool showingTargetDetails: false
    property bool showingAddTarget: false
    property bool addTargetBusy: false
    property real dashboardScrollLocation: 0
    property var loadingTargetAppIds: []
    property var filteredTargets: []
    property int currentPage: 1
    readonly property int pageSize: calculatePageSize()
    property int totalPages: 1
    property int totalTargetCount: 0
    property bool targetsReleasedToTray: false
    property string activeSort: ctxSettings.dashboardToolbarSort
    property bool activeSortDescending: ctxSettings.dashboardToolbarSortDescending
    property var activeFilters: ctxSettings.dashboardToolbarFilters.length > 0 ? ctxSettings.dashboardToolbarFilters : ["none"]
    property string activeSearch: ""
    property var targetDetailsAchievements: []
    property string targetDetailsActiveSort: "unlockDate"
    property bool targetDetailsSortDescending: false
    property var targetDetailsActiveFilters: ["all"]
    readonly property int requiredWindowMinimumWidth: activeLayout === "detailedList" || showingTargetDetails ? 1280 : 900

    ListModel {
        id: id_targetModel
    }

    ListModel {
        id: id_targetDetailsAchievementModel
    }

    Timer {
        id: id_searchRefreshTimer

        interval: 250
        repeat: false
        onTriggered: id_root.refreshTargets()
    }

    Timer {
        id: id_layoutRefreshTimer

        interval: 250
        repeat: false
        onTriggered: id_root.recalculatePaging(false)
    }

    Component.onCompleted: refreshTargets()

    Connections {
        target: ctxSettings

        function onSignalDefaultsReset() {
            id_root.activeLayout = ctxSettings.dashboardToolbarLayout
            id_root.activeSort = ctxSettings.dashboardToolbarSort
            id_root.activeSortDescending = ctxSettings.dashboardToolbarSortDescending
            id_root.activeFilters = ctxSettings.dashboardToolbarFilters.length > 0 ? ctxSettings.dashboardToolbarFilters : ["none"]
            id_root.targetDetailsActiveSort = "unlockDate"
            id_root.targetDetailsSortDescending = false
            id_root.targetDetailsActiveFilters = ["all"]
            id_root.refreshTargets()
            id_root.refreshTargetDetailsAchievements()
        }
    }

    function calculatePageSize() {
        switch (id_root.activeLayout) {
            case "smallCardGrid":
                return id_root.calculateGridPageSize(150, 16, 6)
            case "defaultCardGrid":
                return id_root.calculateGridPageSize(200, 16, 4)
            case "detailedList":
                return 25
            case "list":
                return 50
            default:
                return 50
        }
    }

    function calculateGridPageSize(cardWidth, spacing, rows) {
        const availableWidth = id_cardLayoutLoader ? id_cardLayoutLoader.width : 0
        const columns = Math.max(1, Math.floor((availableWidth + spacing) / (cardWidth + spacing)))
        return Math.max(1, columns * rows)
    }

    function recalculatePaging(resetScroll) {
        id_root.totalPages = Math.max(1, Math.ceil(id_root.totalTargetCount / id_root.pageSize))
        id_root.currentPage = Math.max(1, Math.min(id_root.currentPage, id_root.totalPages))
        id_root.rebuildTargetPage()

        if (resetScroll) {
            id_root.resetDashboardScroll()
        }
    }

    function refreshTargets() {
        id_root.targetsReleasedToTray = false

        const targets = ctxLymalink.FetchDashboardTargets()
        const filteredTargets = id_root.filterTargets(targets)
        id_root.sortTargets(filteredTargets)

        for (let i = 0; i < filteredTargets.length; ++i) {
            filteredTargets[i].isLoading = id_root.isTargetLoading(filteredTargets[i].id, filteredTargets[i].targetType)
        }

        id_root.filteredTargets = filteredTargets
        id_root.totalTargetCount = filteredTargets.length
        id_root.recalculatePaging(false)
        id_root.noTargetsAvailable = id_root.totalTargetCount === 0
    }

    function rebuildTargetPage() {
        id_targetModel.clear()

        const startIndex = (id_root.currentPage - 1) * id_root.pageSize
        const endIndex = Math.min(startIndex + id_root.pageSize, id_root.totalTargetCount)
        for (let i = startIndex; i < endIndex; ++i) {
            const target = id_root.filteredTargets[i]
            target.rowLayout = id_root.activeLayout === "detailedList" ? "detailedList" : "list"
            target.isLoading = id_root.isTargetLoading(target.id, target.targetType)
            id_targetModel.append(target)
        }
    }

    function resetDashboardScroll() {
        id_root.dashboardScrollLocation = 0
        if (id_cardLayoutLoader.status === Loader.Ready
            && id_cardLayoutLoader.item
            && typeof id_cardLayoutLoader.item.restoreScrollLocation === "function") {
            id_cardLayoutLoader.item.restoreScrollLocation(0)
        }
    }

    function goToPage(page) {
        const nextPage = Math.max(1, Math.min(page, id_root.totalPages))
        if (nextPage === id_root.currentPage) {
            return
        }

        id_root.currentPage = nextPage
        id_root.resetDashboardScroll()
        id_root.rebuildTargetPage()
    }

    function pagerItems() {
        const pages = []
        if (id_root.totalPages <= 7) {
            for (let page = 1; page <= id_root.totalPages; ++page) {
                pages.push({ label: page.toString(), page: page, isEllipsis: false })
            }
            return pages
        }

        function appendPage(page) {
            pages.push({ label: page.toString(), page: page, isEllipsis: false })
        }

        function appendEllipsis() {
            pages.push({ label: "...", page: -1, isEllipsis: true })
        }

        appendPage(1)

        const middleStart = Math.max(2, id_root.currentPage - 1)
        const middleEnd = Math.min(id_root.totalPages - 1, id_root.currentPage + 1)

        if (middleStart > 2) {
            appendEllipsis()
        }

        for (let page = middleStart; page <= middleEnd; ++page) {
            appendPage(page)
        }

        if (middleEnd < id_root.totalPages - 1) {
            appendEllipsis()
        }

        appendPage(id_root.totalPages)
        return pages
    }

    function pageRangeLabel() {
        if (id_root.totalTargetCount === 0) {
            return qsTr("Page 0 of 0 - 0 of 0")
        }

        const start = ((id_root.currentPage - 1) * id_root.pageSize) + 1
        const end = Math.min(id_root.currentPage * id_root.pageSize, id_root.totalTargetCount)
        return qsTr("Page %1 of %2 - %3-%4 of %5")
            .arg(id_root.currentPage)
            .arg(id_root.totalPages)
            .arg(start)
            .arg(end)
            .arg(id_root.totalTargetCount)
    }

    function showingDashboardTargets() {
        return !id_root.targetsReleasedToTray
            && !id_root.showingTargetDetails
            && !id_root.showingAddTarget
            && !id_root.noTargetsAvailable
    }

    function releaseVisibleTargetsForTray() {
        id_layoutRefreshTimer.stop()
        id_searchRefreshTimer.stop()
        id_targetModel.clear()
        id_root.targetsReleasedToTray = true
    }

    function restoreVisibleTargetsFromTray() {
        if (id_root.targetsReleasedToTray) {
            id_root.refreshTargets()
        }
    }

    function matchesFilter(target, filter) {
        const achievementCount = Number(target.achievementCount ?? 0)
        const achievementTotal = Number(target.achievementTotal ?? 0)
        const targetType = (target.targetType ?? "").toString().toLocaleLowerCase()
        const status = (target.status ?? "").toString().toLocaleLowerCase()

        switch (filter) {
            case "none":         return true
            case "completed":    return achievementTotal > 0 && achievementCount >= achievementTotal
            case "uncompleted":  return achievementTotal <= 0 || achievementCount < achievementTotal
            case "custom":       return targetType === "custom"
            case "emulator":     return targetType === "emulator"
            case "steam":        return targetType === "steam"
            case "hidden":       return Boolean(target.targetHidden)
            case "installed":    return status === "installed"
            case "notInstalled": return status === "not installed"
            default:             return true
        }
    }

    function filterTargets(targets) {
        const filters = id_root.activeFilters
        const showingHiddenTargets = filters.indexOf("hidden") !== -1
        const searchTerms = id_root.activeSearch
            .trim()
            .toLocaleLowerCase()
            .split(/\s+/)
            .filter(function(term) { return term.length > 0 })

        return targets.filter(function(target) {
            if (filters.indexOf("none") !== -1) {
                if (Boolean(target.targetHidden)) {
                    return false
                }
            } else if (!showingHiddenTargets && Boolean(target.targetHidden)) {
                return false
            }

            if (filters.indexOf("none") === -1) {
                let filterMatched = false
                for (let i = 0; i < filters.length; ++i) {
                    if (id_root.matchesFilter(target, filters[i])) {
                        filterMatched = true
                        break
                    }
                }

                if (!filterMatched) {
                    return false
                }
            }

            return id_root.matchesSearch(target, searchTerms)
        })
    }

    function matchesSearch(target, searchTerms) {
        if (searchTerms.length === 0) {
            return true
        }

        const searchableText = [
            target.title,
            target.targetType,
            target.status,
            target.id
        ].map(function(value) {
            return (value ?? "").toString().toLocaleLowerCase()
        }).join(" ")

        for (let i = 0; i < searchTerms.length; ++i) {
            if (searchableText.indexOf(searchTerms[i]) === -1) {
                return false
            }
        }

        return true
    }

    function sortValue(target, sort) {
        switch (sort) {
            case "title":        return (target.title ?? "").toString().toLocaleLowerCase()
            case "progress":     return Number(target.progressValue ?? 0)
            case "recentUnlock": return Number(target.recentUnlockTimestamp ?? 0)
            case "playtime":     return Number(target.playtimeSeconds ?? 0)
            case "lastPlayed":   return Number(target.lastPlayedTimestamp ?? 0)
            case "dateAdded":    return Number(target.dateAddedTimestamp ?? 0)
            default:             return (target.title ?? "").toString().toLocaleLowerCase()
        }
    }

    function sortTargets(targets) {
        const sort = id_root.activeSort
        const direction = id_root.activeSortDescending ? -1 : 1

        targets.sort(function(left, right) {
            const leftValue = id_root.sortValue(left, sort)
            const rightValue = id_root.sortValue(right, sort)

            if (leftValue < rightValue) return -1 * direction
            if (leftValue > rightValue) return 1 * direction

            const leftTitle = (left.title ?? "").toString().toLocaleLowerCase()
            const rightTitle = (right.title ?? "").toString().toLocaleLowerCase()
            if (leftTitle < rightTitle) return -1
            if (leftTitle > rightTitle) return 1
            return 0
        })
    }

    function matchesTargetDetailsFilter(achievement, filter) {
        switch (filter) {
            case "all":      return true
            case "unlocked": return Boolean(achievement.unlocked)
            case "locked":   return !Boolean(achievement.unlocked) && !Boolean(achievement.achievementHidden)
            case "hidden":   return !Boolean(achievement.unlocked) && Boolean(achievement.achievementHidden)
            default:         return true
        }
    }

    function filterTargetDetailsAchievements(achievements) {
        const filters = id_root.targetDetailsActiveFilters
        if (filters.indexOf("all") !== -1) {
            return achievements
        }

        return achievements.filter(function(achievement) {
            for (let i = 0; i < filters.length; ++i) {
                if (id_root.matchesTargetDetailsFilter(achievement, filters[i])) {
                    return true
                }
            }
            return false
        })
    }

    function targetDetailsSectionRank(sectionKey) {
        switch (sectionKey) {
            case "unlocked":          return 0
            case "locked":            return 1
            case "achievementHidden": return 2
            default:                  return 3
        }
    }

    function targetDetailsSortValue(achievement, sort) {
        switch (sort) {
            case "name":             return (achievement.achievementName ?? "").toString().toLocaleLowerCase()
            case "unlockDate":       return Number(achievement.unlockTimestamp ?? 0)
            case "globalPercentage": return Number(achievement.globalUnlockPercentage ?? 0)
            default:                 return (achievement.achievementName ?? "").toString().toLocaleLowerCase()
        }
    }

    function sortTargetDetailsAchievements(achievements) {
        const sort = id_root.targetDetailsActiveSort
        const descending = sort === "unlockDate"
            ? !id_root.targetDetailsSortDescending
            : id_root.targetDetailsSortDescending
        const direction = descending ? -1 : 1

        achievements.sort(function(left, right) {
            const leftSection = id_root.targetDetailsSectionRank(left.sectionKey)
            const rightSection = id_root.targetDetailsSectionRank(right.sectionKey)
            if (leftSection < rightSection) return -1
            if (leftSection > rightSection) return 1

            const leftValue = id_root.targetDetailsSortValue(left, sort)
            const rightValue = id_root.targetDetailsSortValue(right, sort)
            if (leftValue < rightValue) return -1 * direction
            if (leftValue > rightValue) return 1 * direction

            const leftName = (left.achievementName ?? "").toString().toLocaleLowerCase()
            const rightName = (right.achievementName ?? "").toString().toLocaleLowerCase()
            if (leftName < rightName) return -1
            if (leftName > rightName) return 1
            return 0
        })
    }

    function refreshTargetDetailsAchievements() {
        id_targetDetailsAchievementModel.clear()

        const achievements = id_root.filterTargetDetailsAchievements(id_root.targetDetailsAchievements.slice())
        id_root.sortTargetDetailsAchievements(achievements)
        for (let i = 0; i < achievements.length; ++i) {
            id_targetDetailsAchievementModel.append(achievements[i])
        }
    }

    function syncTargetRowLayout() {
        const rowLayout = id_root.activeLayout === "detailedList" ? "detailedList" : "list"
        for (let i = 0; i < id_targetModel.count; ++i) {
            id_targetModel.setProperty(i, "rowLayout", rowLayout)
        }
    }

    function targetKey(appId, targetType) {
        return appId + ":" + (targetType ?? "Emulator")
    }

    function isTargetLoading(appId, targetType) {
        return loadingTargetAppIds.indexOf(id_root.targetKey(appId, targetType)) !== -1
    }

    function setTargetLoading(appId, targetType, loading) {
        const key = id_root.targetKey(appId, targetType)
        const index = loadingTargetAppIds.indexOf(key)
        if (loading && index === -1) {
            loadingTargetAppIds = loadingTargetAppIds.concat([key])
        } else if (!loading && index !== -1) {
            const nextIds = loadingTargetAppIds.slice()
            nextIds.splice(index, 1)
            loadingTargetAppIds = nextIds
        }

        for (let i = 0; i < id_root.filteredTargets.length; ++i) {
            const target = id_root.filteredTargets[i]
            if (id_root.targetKey(target.id, target.targetType) === key) {
                target.isLoading = loading
                break
            }
        }

        for (let i = 0; i < id_targetModel.count; ++i) {
            const target = id_targetModel.get(i)
            if (id_root.targetKey(target.id, target.targetType) === key) {
                id_targetModel.setProperty(i, "isLoading", loading)
                break
            }
        }
    }

    function saveDashboardScrollLocation() {
        if (id_cardLayoutLoader.status === Loader.Ready
            && id_cardLayoutLoader.item
            && typeof id_cardLayoutLoader.item.currentScrollLocation === "function") {
            id_root.dashboardScrollLocation = id_cardLayoutLoader.item.currentScrollLocation()
        }
    }

    function onTargetSelected(appId, targetType) {
        id_root.saveDashboardScrollLocation()

        const details = ctxLymalink.FetchTargetDetails(appId, targetType)

        id_root.targetDetailsAchievements = details.achievements ?? []
        id_root.refreshTargetDetailsAchievements()

        id_root.pendingTargetDetails = details
        id_root.showingTargetDetails = true
    }

    function reloadTargetDetails(appId, targetType) {
        if (!id_root.pendingTargetDetails || id_root.pendingTargetDetails.id !== appId || id_root.pendingTargetDetails.targetType !== targetType) {
            return
        }

        const details = ctxLymalink.FetchTargetDetails(appId, targetType)
        id_root.targetDetailsAchievements = details.achievements ?? []
        id_root.refreshTargetDetailsAchievements()
        id_root.pendingTargetDetails = details
        id_root.refreshTargets()
    }

    function reloadBackendTargets() {
        if (typeof ctxDBusService !== "undefined" && ctxDBusService !== null) {
            ctxDBusService.ReloadAllTargets()
        }
    }

    function applyAchievementImportResult(addedTargets) {
        id_root.showingTargetDetails = false
        id_root.showingAddTarget = false
        id_root.pendingTargetDetails = null
        id_root.targetDetailsAchievements = []
        id_root.reloadBackendTargets()

        const targets = addedTargets || []
        for (let i = 0; i < targets.length; ++i) {
            const target = targets[i] || {}
            const appId = Number(target.id || 0)
            const targetType = (target.targetType || "Emulator").toString()
            if (appId <= 0) {
                continue
            }

            id_root.setTargetLoading(appId, targetType, true)
            ctxLymalink.EnqueueSteamHydrationTask(appId, true, targetType)
        }

        id_root.refreshTargets()
    }

    Connections {
        target: ctxLymalink

        function onSignalErrorOccurred(title, message) {
            id_errorPopup.showError(title, message)
        }

        function onSignalSteamHydrationTaskStarted(appId, targetType) {
            id_root.setTargetLoading(appId, targetType, true)
        }

        function onSignalSteamHydrationTaskFinished(appId, targetType, success, cancelled) {
            id_root.setTargetLoading(appId, targetType, false)
            id_root.refreshTargets()
            if (success && !cancelled) {
                id_root.reloadBackendTargets()
            }
        }
    }

    Connections {
        target: typeof ctxDBusService !== "undefined" ? ctxDBusService : null

        function onSignalAchievementUnlocked(appId, achievementKey) {
            if (id_root.pendingTargetDetails) {
                id_root.reloadTargetDetails(appId, id_root.pendingTargetDetails.targetType)
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// LAYOUTS //////////////////////////////
    /////////////////////////////////////////////////////////////////////

    Component {
        id: id_cardGridLayout

        CardGrid {
            p_gridSize: id_root.activeLayout
            p_targetModel: id_targetModel
            p_scrollLocation: id_root.dashboardScrollLocation
        }
    }

    Component {
        id: id_cardListLayout
        
        CardList {
            p_listMode: id_root.activeLayout
            p_targetModel: id_targetModel
            p_scrollLocation: id_root.dashboardScrollLocation
        }
    }

    Component {
        id: id_targetDetailsLayout

        TargetDetails {
            p_appId: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.id : 0
            p_targetType: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.targetType : ""
            p_title: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.title : ""
            p_coverSource: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.coverSourceTargetDetails : ""
            p_achievementCount: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.achievementCount : 0
            p_achievementTotal: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.achievementTotal : 0
            p_installationStatus: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.installationStatus : ""
            p_lastPlayed: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.lastPlayed : ""
            p_recentUnlock: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.recentUnlock : ""
            p_playtime: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.playtime : ""
            p_appIdDirFound: id_root.pendingTargetDetails ? Boolean(id_root.pendingTargetDetails.appIdDirFound) : false
            p_emulatorType: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.emulatorType : ""
            p_globalColorStyle: ctxSettings.globalColorStyle
            p_achievementModel: id_targetDetailsAchievementModel

            onAchievementStateChanged: function(appId) {
                id_root.reloadTargetDetails(appId, "Emulator")
            }
        }
    }

    Component {
        id: id_newTargetLayout

        NewTarget {
            onTargetAdded: function(appId, targetType) {
                id_root.showingAddTarget = false
                id_root.showingTargetDetails = false
                id_root.refreshTargets()
                id_root.reloadBackendTargets()
                id_root.setTargetLoading(appId, targetType, true)
                ctxLymalink.EnqueueSteamHydrationTask(appId, true, targetType)
            }
            onSteamImportsApplied: function(loadingAppIds) {
                id_root.addTargetBusy = false
                id_root.showingAddTarget = false
                id_root.showingTargetDetails = false
                id_root.refreshTargets()
                for (let i = 0; i < loadingAppIds.length; ++i) {
                    id_root.setTargetLoading(loadingAppIds[i], "Steam", true)
                }
            }
            onBusyChanged: function(busy) {
                id_root.addTargetBusy = busy
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    ErrorPopup {
        id: id_errorPopup
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        anchors.topMargin: 48
        spacing: 12

        // Dashboard Title and dynamic Toolbar
        DashboardToolbar {
            Layout.fillWidth: true
            p_toolbarTitle: id_root.showingAddTarget
                ? qsTr("Add Target")
                : id_root.showingTargetDetails
                    ? id_root.pendingTargetDetails.title
                    : qsTr("Dashboard")
            p_targetDetailsVisible: id_root.showingTargetDetails ? true : false
            p_addTargetVisible: id_root.showingAddTarget ? true : false
            p_appId: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.id : 0
            p_targetType: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.targetType : ""
            p_targetHidden: id_root.pendingTargetDetails ? Boolean(id_root.pendingTargetDetails.targetHidden) : false
            p_activeLayout: id_root.activeLayout
            p_returnLocked: id_root.addTargetBusy

            // Layout selection
            onLayoutSelected: function(size) {
                id_root.activeLayout = size
                ctxSettings.SaveValue(Settings.DashboardToolbarLayout, size)
                id_root.refreshTargets()
            }

            // Close Target Details
            onReturnClicked: {
                if (id_root.addTargetBusy) {
                    return
                }
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = false
                id_root.addTargetBusy = false
            }

            // Add target
            onAddTargetClicked: {
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = true
                id_root.addTargetBusy = false
            }

            onRefreshClicked: {
                if (id_root.showingTargetDetails && id_root.pendingTargetDetails) {
                    id_root.reloadTargetDetails(id_root.pendingTargetDetails.id, id_root.pendingTargetDetails.targetType)
                } else {
                    id_root.refreshTargets()
                }
            }

            onSortSelected: function(sort) {
                id_root.activeSort = sort
                id_root.refreshTargets()
            }

            onSortOrderSelected: function(descending) {
                id_root.activeSortDescending = descending
                id_root.refreshTargets()
            }

            onFiltersSelected: function(filters) {
                id_root.activeFilters = filters
                id_root.refreshTargets()
            }

            onSearchTextChanged: function(text) {
                id_root.activeSearch = text
                id_searchRefreshTimer.restart()
            }

            onTargetDetailsSortSelected: function(sort) {
                id_root.targetDetailsActiveSort = sort
                id_root.refreshTargetDetailsAchievements()
            }

            onTargetDetailsSortOrderSelected: function(descending) {
                id_root.targetDetailsSortDescending = descending
                id_root.refreshTargetDetailsAchievements()
            }

            onTargetDetailsFiltersSelected: function(filters) {
                id_root.targetDetailsActiveFilters = filters
                id_root.refreshTargetDetailsAchievements()
            }

            onReloadAssetsRequested: function(appId, targetType) {
                id_root.setTargetLoading(appId, targetType, true)
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = false
            }

            onTargetDataUpdated: function(appId, targetType) {
                id_root.reloadTargetDetails(appId, targetType)
                id_root.reloadBackendTargets()
            }

            onTargetHiddenChanged: function(appId, targetType, hidden) {
                if (id_root.pendingTargetDetails && id_root.pendingTargetDetails.id === appId && id_root.pendingTargetDetails.targetType === targetType) {
                    id_root.pendingTargetDetails = Object.assign({}, id_root.pendingTargetDetails, { targetHidden: hidden })
                }
                id_root.refreshTargets()
            }

            onTargetDeleted: function(appId, targetType) {
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = false
                id_root.pendingTargetDetails = null
                id_root.targetDetailsAchievements = []
                id_root.refreshTargets()
                id_root.reloadBackendTargets()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Themes.dashboard.colors.divider
        }

        // Empty state
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: id_root.noTargetsAvailable && !id_root.showingAddTarget && !id_root.showingTargetDetails

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width, 520)
                spacing: 18

                CustomBusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    p_indicatorSize: 280
                    p_speed: 8400
                    p_running: id_root.noTargetsAvailable
                    opacity: 0.5
                }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("All quiet here, nothing to show.")
                    font.pixelSize: Themes.dashboard.fontSizes.emptyTitle
                    font.bold: true
                    color: Themes.dashboard.colors.titleText
                    opacity: 0.4
                }
            }
        }

        // Main content: CardGrid or CardList
        Loader {
            id: id_cardLayoutLoader

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !id_root.noTargetsAvailable || id_root.showingAddTarget || id_root.showingTargetDetails
            sourceComponent: {
                if (id_root.showingAddTarget) {
                    return id_newTargetLayout
                } else if (id_root.showingTargetDetails) {
                    return id_targetDetailsLayout
                } else if (id_root.activeLayout === "list" || id_root.activeLayout === "detailedList") {
                    return id_cardListLayout
                } else if (id_root.activeLayout === "smallCardGrid" || id_root.activeLayout === "defaultCardGrid") {
                    return id_cardGridLayout
                } else {
                    console.error("Dashboard - sourceComponent not defined")
                }  
            }

            onLoaded: {
                if (typeof item.openTargetDetails !== "undefined") {
                    item.openTargetDetails.connect(id_root.onTargetSelected)
                }
            }

            onWidthChanged: {
                if (id_root.activeLayout === "smallCardGrid" || id_root.activeLayout === "defaultCardGrid") {
                    id_layoutRefreshTimer.restart()
                }
            }
        }

        // Dashboard pager
        Item {
            id: id_pager

            Layout.fillWidth: true
            implicitHeight: id_pager.visible ? 36 : 0
            visible: id_root.showingDashboardTargets() && id_root.totalPages > 1

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Rectangle {
                    id: id_previousPage

                    implicitWidth: 34
                    implicitHeight: 32
                    radius: 16
                    enabled: id_root.currentPage > 1
                    opacity: enabled ? 1.0 : 0.38
                    color: id_previousPageMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : id_previousPageMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                    border.width: 1
                    border.color: Themes.dashboardToolbar.colors.pillBorder

                    Text {
                        anchors.centerIn: parent
                        text: "<"
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }

                    MouseArea {
                        id: id_previousPageMouseArea

                        anchors.fill: parent
                        enabled: id_previousPage.enabled
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: id_root.goToPage(id_root.currentPage - 1)
                    }
                }

                Repeater {
                    model: id_root.pagerItems()

                    Rectangle {
                        required property var modelData

                        implicitWidth: modelData.isEllipsis ? 24 : 34
                        implicitHeight: 32
                        radius: 16
                        enabled: !modelData.isEllipsis
                        color: modelData.page === id_root.currentPage
                            ? Themes.dashboardToolbar.colors.segmentActive
                            : id_pageMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.pillPressed
                                : id_pageMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.pillHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: modelData.isEllipsis ? 0 : 1
                        border.color: modelData.page === id_root.currentPage
                            ? Themes.dashboardToolbar.colors.pillBorderOpen
                            : Themes.dashboardToolbar.colors.pillBorder

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: modelData.page === id_root.currentPage
                                ? Themes.dashboardToolbar.colors.segmentLabelActive
                                : Themes.dashboardToolbar.colors.pillLabel
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                        }

                        MouseArea {
                            id: id_pageMouseArea

                            anchors.fill: parent
                            enabled: !modelData.isEllipsis
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: id_root.goToPage(modelData.page)
                        }
                    }
                }

                Rectangle {
                    id: id_nextPage

                    implicitWidth: 34
                    implicitHeight: 32
                    radius: 16
                    enabled: id_root.currentPage < id_root.totalPages
                    opacity: enabled ? 1.0 : 0.38
                    color: id_nextPageMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : id_nextPageMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                    border.width: 1
                    border.color: Themes.dashboardToolbar.colors.pillBorder

                    Text {
                        anchors.centerIn: parent
                        text: ">"
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }

                    MouseArea {
                        id: id_nextPageMouseArea

                        anchors.fill: parent
                        enabled: id_nextPage.enabled
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: id_root.goToPage(id_root.currentPage + 1)
                    }
                }

                Label {
                    text: id_root.pageRangeLabel()
                    color: Themes.dashboardToolbar.colors.pillLabel
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                }
            }
        }

        // Binding {
        //     when: !id_root.showingTargetDetails
        //         && id_root.activeLayout !== "list"
        //         && id_root.activeLayout !== "detailedList"
        //         && id_cardLayoutLoader.status === Loader.Ready
        //         && id_cardLayoutLoader.sourceComponent === id_cardGridLayout
        //     target: id_cardLayoutLoader.item
        //     property: "p_gridSize"
        //     value: id_root.activeLayout
        // }
    }
}
