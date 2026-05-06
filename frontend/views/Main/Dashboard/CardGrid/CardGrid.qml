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
    id: root

    // "default" | "small"
    property string gridSize: "default"

    // Internals
    readonly property int cellW: root.gridSize === "default" ? 200 : 150
    readonly property int cellH: root.gridSize === "default" ? 300 : 225
    readonly property int cellSpacing: 16
    readonly property bool hasVerticalScroll: scrollView.ScrollBar.vertical.size < 1.0

    // TERMPORARY: Dummy model
    ListModel {
        id: dummyModel

        ListElement { title: "Hollow Warden";           coverSource: "qrc:/qt/qml/Lymalink/res/img/library_capsule_2x.jpg"; achievementCount: 45; achievementTotal: 63; status: "Installed"; lastPlayed: "2 days ago" }
        ListElement { title: "Frostpeak";               coverSource: ""; achievementCount: 12; achievementTotal: 24; status: "Installed"; lastPlayed: "1 week ago" }
        ListElement { title: "Acheron";                 coverSource: ""; achievementCount: 0; achievementTotal: 49; status: "Not Installed"; lastPlayed: "" }
        ListElement { title: "Dissonant Reverie";       coverSource: ""; achievementCount: 8; achievementTotal: 27; status: "Installed"; lastPlayed: "Yesterday" }
        ListElement { title: "Twilight Hollow";         coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; achievementCount: 30; achievementTotal: 40; status: "Not Installed"; lastPlayed: "3 days ago" }
        ListElement { title: "The Lost Meridian";       coverSource: ""; achievementCount: 0; achievementTotal: 16; status: "Not Installed"; lastPlayed: "" }
        ListElement { title: "Aris and the Shroudwood"; coverSource: ""; achievementCount: 20; achievementTotal: 35; status: "Installed"; lastPlayed: "5 days ago" }
        ListElement { title: "The Mischievous Fowl";    coverSource: ""; achievementCount: 5; achievementTotal: 12; status: "Installed"; lastPlayed: "2 weeks ago" }
        ListElement { title: "Aethelwald III";          coverSource: ""; achievementCount: 102; achievementTotal: 102; status: "Installed"; lastPlayed: "1 hour ago" }
    }

    ScrollView {
        id: scrollView
        
        anchors.fill: parent
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        contentHeight: flowContainer.height  // explicit so ScrollView knows when to show the scrollbar
        clip: true

        // Wraps Flow so we can animate implicitHeight changes (Flow's is read-only)
        Item {
            id: flowContainer

            width: scrollView.availableWidth - (root.hasVerticalScroll ? 30 : 0)
            height: flow.implicitHeight + flow.topPadding

            Behavior on height {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutQuad
                }
            }

            // Flow preserves delegate identity across re-layouts, enabling move transitions
            Flow {
                id: flow

                width: parent.width
                spacing: root.cellSpacing
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
                    model: dummyModel

                    Item {
                        id: cellItem
                        
                        width: root.cellW
                        height: root.cellH

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
                            sourceComponent: root.gridSize === "default" ? defaultCoverCard : smallCoverCard

                            onLoaded: {
                                item.title            = model.title
                                item.coverSource      = model.coverSource
                                item.achievementCount = model.achievementCount
                                item.achievementTotal = model.achievementTotal
                                item.status           = model.status
                                item.lastPlayed       = model.lastPlayed
                            }
                        }
                    }
                }
            }
        }
    }

    // Card components
    Component {
        id: defaultCoverCard

        Card {
            showEdgeProgressFrame: true
            showMiniAchievementsBadge: true
        }
    }

    Component {
        id: smallCoverCard

        CardSmall {
            showEdgeProgressFrame: true
            showMiniAchievementsBadge: true
        }
    }
}