/////////////////////////////////////////////////////////
// File: Dashboard.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Dashboard displaying tracked content.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Internals _____________________________________________
    property string activeLayout: "defaultCardGrid" // list, detailedList, smallCardGrid, defaultCardGrid
    property bool noTargetsAvailable: false
    property var pendingTargetDetails: null
    property bool showingTargetDetails: false
    property bool showingAddTarget: false
    readonly property int requiredWindowMinimumWidth: activeLayout === "detailedList" || showingTargetDetails ? 1280 : 900

    function onTargetSelected(title, coverSource, achievementCount, achievementTotal, installationStatus, lastPlayed, recentUnlock) {
        id_root.pendingTargetDetails = {
            title: title,
            coverSource: coverSource,
            achievementCount: achievementCount,
            achievementTotal: achievementTotal,
            installationStatus: installationStatus,
            lastPlayed: lastPlayed,
            recentUnlock: recentUnlock
        }
        id_root.showingTargetDetails = true
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// LAYOUTS //////////////////////////////
    /////////////////////////////////////////////////////////////////////

    Component {
        id: id_cardGridLayout

        CardGrid {
            p_gridSize: id_root.activeLayout
        }
    }

    Component {
        id: id_cardListLayout
        
        CardList {
            p_listMode: id_root.activeLayout
        }
    }

    Component {
        id: id_targetDetailsLayout

        TargetDetails {
            p_title: id_root.pendingTargetDetails?.title ?? ""
            p_coverSource: id_root.pendingTargetDetails?.coverSource ?? ""
            p_achievementCount: id_root.pendingTargetDetails?.achievementCount ?? 0
            p_achievementTotal: id_root.pendingTargetDetails?.achievementTotal ?? 0
            p_installationStatus: id_root.pendingTargetDetails?.installationStatus ?? ""
            p_lastPlayed: id_root.pendingTargetDetails?.lastPlayed ?? ""
            p_recentUnlock: id_root.pendingTargetDetails?.recentUnlock ?? ""
            p_globalColorStyle: ctxSettings.globalColorStyle
        }
    }

    Component {
        id: id_newTargetLayout

        NewTarget {
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
            p_activeLayout: id_root.activeLayout

            // Layout selection
            onLayoutSelected: (size) => id_root.activeLayout = size

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
