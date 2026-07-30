/////////////////////////////////////////////////////////
// File: CardList.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: List view for Dashboard. Displays tracked
//              content as horizontal rows via CardRow.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQml.Models

Item {
    id: id_root

    // Public ________________________________________________
    property string p_listMode: "list"
    property var p_targetModel: null
    property real p_scrollLocation: 0

    signal openTargetDetails(int appId, string targetType)
    signal backgroundClicked()

    // Internals _____________________________________________
    readonly property string rowLayout: p_listMode === "detailedList" ? "detailedList" : "list"
    property bool restoringScrollLocation: false

    onRowLayoutChanged: syncRowLayout()
    onP_targetModelChanged: syncRowLayout()
    Component.onCompleted: {
        syncRowLayout()
        Qt.callLater(id_root.restoreScrollLocation)
    }

    function syncRowLayout() {
        if (!id_root.p_targetModel
            || typeof id_root.p_targetModel.setProperty !== "function"
            || typeof id_root.p_targetModel.count !== "number") {
            return
        }

        for (let i = 0; i < id_root.p_targetModel.count; ++i) {
            id_root.p_targetModel.setProperty(i, "rowLayout", id_root.rowLayout)
        }
    }

    function currentScrollLocation() {
        return id_listView.contentY
    }

    function restoreScrollLocation(scrollLocation) {
        if (scrollLocation !== undefined) {
            id_root.p_scrollLocation = scrollLocation
        }

        id_root.restoringScrollLocation = true
        id_listView.contentY = Math.max(
            0,
            Math.min(id_root.p_scrollLocation, id_listView.contentHeight - id_listView.height)
        )
        id_root.restoringScrollLocation = false
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    ///////////////////////////////////////////////////////////////////// 

    // List view
    ListView {
        id: id_listView

        anchors.fill: parent
        spacing: 6
        clip: true

        model: id_root.p_targetModel

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: id_root.backgroundClicked()
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: DelegateChooser {
            role: "rowLayout"

            DelegateChoice {
                roleValue: "detailedList"

                CardRowDetailed {
                    width:              id_listView.width
                    p_appId:            model.id
                    p_title:            model.title
                    p_coverSource:      model.coverSourceRowDetailed
                    p_achievementCount: model.achievementCount
                    p_achievementTotal: model.achievementTotal
                    p_targetType:       model.targetType
                    p_installationStatus:   model.status
                    p_lastPlayed:       model.lastPlayed
                    p_recentUnlock:     model.recentUnlock
                    p_lastAchievementIcon: model.lastAchievementIcon
                    p_lastAchievementName: model.lastAchievementName
                    p_lastAchievementDesc: model.lastAchievementDesc
                    p_isLoading:        model.isLoading
                    p_delegateIndex:    index
                    p_globalColorStyle: ctxSettings.globalColorStyle

                    onOpenTargetDetails: function(appId, targetType) {
                        id_root.openTargetDetails(appId, targetType)
                    }
                }
            }

            DelegateChoice {
                roleValue: "list"

                CardRow {
                    width:              id_listView.width
                    p_appId:            model.id
                    p_title:            model.title
                    p_coverSource:      model.coverSourceCardSmall
                    p_logoSource:       model.logoSource
                    p_achievementCount: model.achievementCount
                    p_achievementTotal: model.achievementTotal
                    p_targetType:       model.targetType
                    p_installationStatus:   model.status
                    p_lastPlayed:       model.lastPlayed
                    p_recentUnlock:     model.recentUnlock
                    p_isLoading:        model.isLoading
                    p_delegateIndex:    index
                    p_globalColorStyle: ctxSettings.globalColorStyle

                    onOpenTargetDetails: function(appId, targetType) {
                        id_root.openTargetDetails(appId, targetType)
                    }
                }
            }
        }
    }
}
