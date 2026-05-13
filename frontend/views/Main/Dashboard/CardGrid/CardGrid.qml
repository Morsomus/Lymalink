/////////////////////////////////////////////////////////
// File: CardGrid.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Grid view for Dashboard. Displays tracked
//              content as cover cards in multiple sizes.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Public ________________________________________________
    property string p_gridSize: "defaultCardGrid"   // "defaultCardGrid" | "smallCardGrid"

    signal openTargetDetails(string title, string coverSource, int achievementCount, int achievementTotal, string status, string lastPlayed, string recentUnlock)

    // Internals _____________________________________________
    readonly property int cellW: id_root.p_gridSize === "defaultCardGrid" ? 200 : 150
    readonly property int cellH: id_root.p_gridSize === "defaultCardGrid" ? 300 : 225
    readonly property int cellSpacing: 16
    readonly property bool hasVerticalScroll: id_rootScrollView.ScrollBar.vertical.size < 1.0

    // TERMPORARY: Dummy model
    ListModel {
        id: id_dummyModel

        ListElement { title: "Hollow Warden";           coverSource: "qrc:/qt/qml/Lymalink/res/img/library_capsule_2x.jpg"; achievementCount: 45; achievementTotal: 63; status: "Installed"; lastPlayed: "2 days ago"; recentUnlock: "1 hour ago";  lastAchievementIcon: ""; lastAchievementName: "Into the Depths"; lastAchievementDesc: "Descend below the third sanctum." }
        ListElement { title: "Frostpeak";               coverSource: ""; achievementCount: 12; achievementTotal: 24; status: "Installed"; lastPlayed: "1 week ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Cold Blooded"; lastAchievementDesc: "Survive the blizzard without shelter." }
        ListElement { title: "Acheron";                 coverSource: ""; achievementCount: 0;  achievementTotal: 49; status: "Not Installed"; lastPlayed: ""; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: ""; lastAchievementDesc: "" }
        ListElement { title: "Dissonant Reverie";       coverSource: ""; achievementCount: 8;  achievementTotal: 27; status: "Installed"; lastPlayed: "Yesterday"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Echo Chamber"; lastAchievementDesc: "Hear all seven memory fragments." }
        ListElement { title: "Twilight Hollow";         coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; achievementCount: 30; achievementTotal: 40; status: "Installed"; lastPlayed: "3 days ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Dusk Wanderer"; lastAchievementDesc: "Explore every region at nightfall." }
        ListElement { title: "The Lost Meridian";       coverSource: ""; achievementCount: 0;  achievementTotal: 16; status: "Not Installed"; lastPlayed: ""; recentUnlock: "2 days ago"; lastAchievementIcon: ""; lastAchievementName: ""; lastAchievementDesc: "" }
        ListElement { title: "Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood Aris and the Shroudwood"; coverSource: ""; achievementCount: 20; achievementTotal: 35; status: "Installed"; lastPlayed: "5 days ago"; recentUnlock: "2 months ago"; lastAchievementIcon: ""; lastAchievementName: "Rootbound"; lastAchievementDesc: "Befriend the ancient grove spirit." }
        ListElement { title: "The Mischievous Fowl";    coverSource: ""; achievementCount: 5;  achievementTotal: 12; status: "Installed"; lastPlayed: "2 weeks ago"; recentUnlock: ""; lastAchievementIcon: ""; lastAchievementName: "Feathered Fury"; lastAchievementDesc: "Defeat an enemy using only the peck." }
        ListElement { title: "Aethelwald III";          coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; achievementCount: 102; achievementTotal: 102; status: "Installed"; lastPlayed: "1 hour ago"; recentUnlock: "4 weeks ago"; lastAchievementIcon: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; lastAchievementName: "The Long Road"; lastAchievementDesc: "Complete every quest in all three kingdoms." }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// LAYOUTS //////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Card components
    Component {
        id: id_defaultCoverCard

        Card {
            p_miniAchievementsBadgeEnabled: true
            p_edgeProgressFrameEnabled: true
            p_edgeProgressFrameStaticGrayColor: false
            p_edgeProgressFrameCompletionAnimation: true
        }
    }

    Component {
        id: id_smallCoverCard

        CardSmall {
            p_miniAchievementsBadgeEnabled: true
            p_edgeProgressFrameEnabled: true
            p_edgeProgressFrameStaticGrayColor: false
            p_edgeProgressFrameCompletionAnimation: true
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    ScrollView {
        id: id_rootScrollView
        
        anchors.fill: parent
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        contentHeight: id_flowContainer.height  // explicit so ScrollView knows when to show the scrollbar
        clip: true

        // Wraps Flow so we can animate implicitHeight changes (Flow's is read-only)
        Item {
            id: id_flowContainer

            width: id_rootScrollView.availableWidth - (id_root.hasVerticalScroll ? 30 : 0)
            height: id_flow.implicitHeight + id_flow.topPadding

            Behavior on height {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutQuad
                }
            }

            // Flow preserves delegate identity across re-layouts, enabling move transitions
            Flow {
                id: id_flow

                width: parent.width
                spacing: id_root.cellSpacing
                topPadding: 6

                // Slide cards to new positions on re-wrap
                move: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 220
                        easing.type:
                        Easing.OutQuad
                    }
                }

                Repeater {
                    model: id_dummyModel

                    Item {
                        width: id_root.cellW
                        height: id_root.cellH

                        Behavior on width  {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }

                        Behavior on height {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }

                        Loader {
                            anchors.fill: parent
                            sourceComponent: id_root.p_gridSize === "defaultCardGrid" ? id_defaultCoverCard : id_smallCoverCard

                            onLoaded: {
                                item.p_title            = model.title
                                item.p_coverSource      = model.coverSource
                                item.p_achievementCount = model.achievementCount
                                item.p_achievementTotal = model.achievementTotal
                                item.p_status           = model.status
                                item.p_lastPlayed       = model.lastPlayed
                                item.p_recentUnlock     = model.recentUnlock
                                item.openTargetDetails.connect(id_root.openTargetDetails)
                            }
                        }
                    }
                }
            }
        }
    }
}