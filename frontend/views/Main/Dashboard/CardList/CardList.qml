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

    signal openTargetDetails(string title, string coverSource, int achievementCount, int achievementTotal, string status, string lastPlayed, string recentUnlock)

    // Internals _____________________________________________
    readonly property string rowLayout: p_listMode === "detailedList" ? "detailedList" : "list"

    // TERMPORARY: Dummy model
    ListModel {
        id: id_dummyModel

        ListElement { rowLayout: "list"; title: "Hollow Warden";           coverSource: ""; logoSource: ""; achievementCount: 45; achievementTotal: 63; status: "Installed"; lastPlayed: "2 days ago"; recentUnlock: "1 hour ago";  lastAchievementIcon: ""; lastAchievementName: "Into the Depths"; lastAchievementDesc: "Descend below the third sanctum." }
        ListElement { rowLayout: "list"; title: "Frostpeak";               coverSource: ""; logoSource: ""; achievementCount: 12; achievementTotal: 24; status: "Installed"; lastPlayed: "1 week ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Cold Blooded"; lastAchievementDesc: "Survive the blizzard without shelter." }
        ListElement { rowLayout: "list"; title: "Acheron";                 coverSource: ""; logoSource: ""; achievementCount: 0;  achievementTotal: 49; status: "Not Installed"; lastPlayed: ""; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: ""; lastAchievementDesc: "" }
        ListElement { rowLayout: "list"; title: "Dissonant Reverie";       coverSource: ""; logoSource: ""; achievementCount: 8;  achievementTotal: 27; status: "Installed"; lastPlayed: "Yesterday"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Echo Chamber"; lastAchievementDesc: "Hear all seven memory fragments." }
        ListElement { rowLayout: "list"; title: "Twilight Hollow";         coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; logoSource: ""; achievementCount: 30; achievementTotal: 40; status: "Installed"; lastPlayed: "3 days ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Dusk Wanderer"; lastAchievementDesc: "Explore every region at nightfall." }
        ListElement { rowLayout: "list"; title: "The Lost Meridian";       coverSource: ""; logoSource: ""; achievementCount: 0;  achievementTotal: 16; status: "Not Installed"; lastPlayed: ""; recentUnlock: "2 days ago"; lastAchievementIcon: ""; lastAchievementName: ""; lastAchievementDesc: "" }
        ListElement { rowLayout: "list"; title: "Aris and the Shroudwood"; coverSource: ""; logoSource: ""; achievementCount: 20; achievementTotal: 35; status: "Installed"; lastPlayed: "5 days ago"; recentUnlock: "2 months ago"; lastAchievementIcon: ""; lastAchievementName: "Rootbound"; lastAchievementDesc: "Befriend the ancient grove spirit." }
        ListElement { rowLayout: "list"; title: "The Mischievous Fowl";    coverSource: ""; logoSource: ""; achievementCount: 5;  achievementTotal: 12; status: "Installed"; lastPlayed: "2 weeks ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Feathered Fury"; lastAchievementDesc: "Defeat an enemy using only the peck." }
        ListElement { rowLayout: "list"; title: "Aethelwald III";          coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; logoSource: ""; achievementCount: 102; achievementTotal: 102; status: "Installed"; lastPlayed: "1 hour ago"; recentUnlock: "4 weeks ago"; lastAchievementIcon: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; lastAchievementName: "The Long Road"; lastAchievementDesc: "Complete every quest in all three kingdoms." }
    }

    onRowLayoutChanged: syncRowLayout()
    Component.onCompleted: syncRowLayout()

    function syncRowLayout() {
        for (let i = 0; i < id_dummyModel.count; ++i) {
            id_dummyModel.setProperty(i, "rowLayout", id_root.rowLayout)
        }
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

        model: id_dummyModel

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: DelegateChooser {
            role: "rowLayout"

            DelegateChoice {
                roleValue: "detailedList"

                CardRowDetailed {
                    width:            id_listView.width
                    p_title:            model.title
                    p_coverSource:      model.coverSource
                    p_achievementCount: model.achievementCount
                    p_achievementTotal: model.achievementTotal
                    p_installationStatus:   model.status
                    p_lastPlayed:       model.lastPlayed
                    p_recentUnlock:     model.recentUnlock
                    p_lastAchievementIcon: model.lastAchievementIcon
                    p_lastAchievementName: model.lastAchievementName
                    p_lastAchievementDesc: model.lastAchievementDesc
                    p_delegateIndex:    index

                    onOpenTargetDetails: function(title, coverSource, achievementCount, achievementTotal, status, lastPlayed, recentUnlock) {
                        id_root.openTargetDetails(
                            title,
                            coverSource,
                            achievementCount,
                            achievementTotal,
                            status,
                            lastPlayed,
                            recentUnlock
                        )
                    }
                }
            }

            DelegateChoice {
                roleValue: "list"

                CardRow {
                    width:            id_listView.width
                    p_title:            model.title
                    p_coverSource:      model.coverSource
                    p_logoSource:       model.logoSource
                    p_achievementCount: model.achievementCount
                    p_achievementTotal: model.achievementTotal
                    p_installationStatus:   model.status
                    p_lastPlayed:       model.lastPlayed
                    p_recentUnlock:     model.recentUnlock
                    p_delegateIndex:    index

                    onOpenTargetDetails: function(title, coverSource, achievementCount, achievementTotal, status, lastPlayed, recentUnlock) {
                        id_root.openTargetDetails(
                            title,
                            coverSource,
                            achievementCount,
                            achievementTotal,
                            status,
                            lastPlayed,
                            recentUnlock
                        )
                    }
                }
            }
        }
    }
}
