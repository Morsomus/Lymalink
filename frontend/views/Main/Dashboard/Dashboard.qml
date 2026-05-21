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
    property var loadingTargetAppIds: []
    readonly property int requiredWindowMinimumWidth: activeLayout === "detailedList" || showingTargetDetails ? 1280 : 900

    ListModel {
        id: id_targetModel
    }

    ListModel {
        id: id_targetDetailsAchievementModel
    }

    Component.onCompleted: refreshTargets()

    onActiveLayoutChanged: syncTargetRowLayout()

    function refreshTargets() {
        id_targetModel.clear()

        const targets = ctxLymalink.FetchDashboardTargets()
        for (let i = 0; i < targets.length; ++i) {
            const target = targets[i]
            target.rowLayout = id_root.activeLayout === "detailedList" ? "detailedList" : "list"
            target.isLoading = id_root.isTargetLoading(target.id)
            id_targetModel.append(target)
        }

        id_root.noTargetsAvailable = id_targetModel.count === 0
    }

    function syncTargetRowLayout() {
        const rowLayout = id_root.activeLayout === "detailedList" ? "detailedList" : "list"
        for (let i = 0; i < id_targetModel.count; ++i) {
            id_targetModel.setProperty(i, "rowLayout", rowLayout)
        }
    }

    function isTargetLoading(appId) {
        return loadingTargetAppIds.indexOf(appId) !== -1
    }

    function setTargetLoading(appId, loading) {
        const index = loadingTargetAppIds.indexOf(appId)
        if (loading && index === -1) {
            loadingTargetAppIds = loadingTargetAppIds.concat([appId])
        } else if (!loading && index !== -1) {
            const nextIds = loadingTargetAppIds.slice()
            nextIds.splice(index, 1)
            loadingTargetAppIds = nextIds
        }

        for (let i = 0; i < id_targetModel.count; ++i) {
            if (id_targetModel.get(i).id === appId) {
                id_targetModel.setProperty(i, "isLoading", loading)
                break
            }
        }
    }

    function onTargetSelected(appId) {
        const details = ctxLymalink.FetchTargetDetails(appId)

        id_targetDetailsAchievementModel.clear()
        const achievements = details.achievements ?? []
        for (let i = 0; i < achievements.length; ++i) {
            id_targetDetailsAchievementModel.append(achievements[i])
        }

        id_root.pendingTargetDetails = details
        id_root.showingTargetDetails = true
    }

    Connections {
        target: ctxLymalink

        function onSignalSteamHydrationTaskStarted(appId) {
            id_root.setTargetLoading(appId, true)
        }

        function onSignalSteamHydrationTaskFinished(appId, success, cancelled) {
            id_root.setTargetLoading(appId, false)
            id_root.refreshTargets()
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
        }
    }

    Component {
        id: id_cardListLayout
        
        CardList {
            p_listMode: id_root.activeLayout
            p_targetModel: id_targetModel
        }
    }

    Component {
        id: id_targetDetailsLayout

        TargetDetails {
            p_title: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.title : ""
            p_coverSource: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.coverSourceTargetDetails : ""
            p_achievementCount: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.achievementCount : 0
            p_achievementTotal: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.achievementTotal : 0
            p_targetType: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.targetType : ""
            p_installationStatus: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.installationStatus : ""
            p_lastPlayed: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.lastPlayed : ""
            p_recentUnlock: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.recentUnlock : ""
            p_playtime: id_root.pendingTargetDetails ? id_root.pendingTargetDetails.playtime : ""
            p_globalColorStyle: ctxSettings.globalColorStyle
            p_achievementModel: id_targetDetailsAchievementModel
        }
    }

    Component {
        id: id_newTargetLayout

        NewTarget {
            onTargetAdded: function(appId) {
                id_root.showingAddTarget = false
                id_root.showingTargetDetails = false
                id_root.refreshTargets()
                id_root.setTargetLoading(appId, true)
                ctxLymalink.EnqueueSteamHydrationTask(appId, true)
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

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
            p_activeLayout: id_root.activeLayout

            // Layout selection
            onLayoutSelected: function(size) {
                id_root.activeLayout = size
                ctxSettings.SaveValue(Settings.DashboardToolbarLayout, size)
            }

            // Close Target Details
            onReturnClicked: {
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = false
            }

            // Add target
            onAddTargetClicked: {
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = true
            }

            onRefreshClicked: id_root.refreshTargets()

            onReloadAssetsRequested: function(appId) {
                id_root.setTargetLoading(appId, true)
                id_root.showingTargetDetails = false
                id_root.showingAddTarget = false
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
                    indicatorSize: 280
                    speed: 8400
                    running: id_root.noTargetsAvailable
                    opacity: 0.5
                }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("All quiet here, nothing to track yet.")
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
