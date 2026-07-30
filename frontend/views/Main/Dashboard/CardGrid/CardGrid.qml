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
    property var p_targetModel: null
    property real p_scrollLocation: 0

    signal openTargetDetails(int appId, string targetType)
    signal backgroundClicked()

    // Internals _____________________________________________
    readonly property int cellW: id_root.p_gridSize === "defaultCardGrid" ? 200 : 150
    readonly property int cellH: id_root.p_gridSize === "defaultCardGrid" ? 300 : 225
    readonly property int cellSpacing: 16
    readonly property bool hasVerticalScroll: id_rootScrollView.ScrollBar.vertical.size < 1.0
    property bool restoringScrollLocation: false

    Component.onCompleted: Qt.callLater(id_root.restoreScrollLocation)

    function currentScrollLocation() {
        return id_rootScrollView.contentItem ? id_rootScrollView.contentItem.contentY : 0
    }

    function restoreScrollLocation(scrollLocation) {
        if (scrollLocation !== undefined) {
            id_root.p_scrollLocation = scrollLocation
        }

        if (!id_rootScrollView.contentItem) {
            return
        }

        id_root.restoringScrollLocation = true
        id_rootScrollView.contentItem.contentY = Math.max(
            0,
            Math.min(id_root.p_scrollLocation, id_rootScrollView.contentItem.contentHeight - id_rootScrollView.contentItem.height)
        )
        id_root.restoringScrollLocation = false
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// LAYOUTS //////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Card components
    Component {
        id: id_defaultCoverCard

        Card {
            p_miniAchievementsBadgeEnabled: ctxSettings.showTotalAchievementsBadge
            p_targetTypeBadgeEnabled: ctxSettings.targetTypeBadgeColorStyle >= 0
            p_targetTypeBadgeColorStyle: ctxSettings.targetTypeBadgeColorStyle
            p_edgeProgressFrameEnabled: ctxSettings.progressFrameColorStyle !== -1
            p_edgeProgressFrameColorStyle: ctxSettings.progressFrameColorStyle
            p_edgeProgressFrameStaticGrayColor: ctxSettings.progressFrameColorStyle === -2
            p_edgeProgressFrameCompletionAnimation: ctxSettings.enableProgressFrameCompletionAnimation
            p_progressBarEnabled: ctxSettings.progressBarColorStyle >= 0
            p_progressBarColorStyle: ctxSettings.progressBarColorStyle
        }
    }

    Component {
        id: id_smallCoverCard

        CardSmall {
            p_miniAchievementsBadgeEnabled: ctxSettings.showTotalAchievementsBadge
            p_targetTypeBadgeEnabled: ctxSettings.targetTypeBadgeColorStyle >= 0
            p_targetTypeBadgeColorStyle: ctxSettings.targetTypeBadgeColorStyle
            p_edgeProgressFrameEnabled: ctxSettings.progressFrameColorStyle !== -1
            p_edgeProgressFrameColorStyle: ctxSettings.progressFrameColorStyle
            p_edgeProgressFrameStaticGrayColor: ctxSettings.progressFrameColorStyle === -2
            p_edgeProgressFrameCompletionAnimation: ctxSettings.enableProgressFrameCompletionAnimation
            p_progressBarEnabled: ctxSettings.progressBarColorStyle >= 0
            p_progressBarColorStyle: ctxSettings.progressBarColorStyle
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

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: id_root.backgroundClicked()
                }

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
                    model: id_root.p_targetModel

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
                            id: id_cardLoader

                            anchors.fill: parent
                            sourceComponent: id_root.p_gridSize === "defaultCardGrid" ? id_defaultCoverCard : id_smallCoverCard

                            onLoaded: {
                                item.p_appId            = model.id
                                item.p_title            = model.title
                                item.p_coverSource      = id_root.p_gridSize === "defaultCardGrid"
                                    ? model.coverSourceCard
                                    : model.coverSourceCardSmall
                                item.p_achievementCount = model.achievementCount
                                item.p_achievementTotal = model.achievementTotal
                                item.p_targetType       = model.targetType
                                item.p_installationStatus   = model.status
                                item.p_lastPlayed       = model.lastPlayed
                                item.p_recentUnlock     = model.recentUnlock
                                item.p_isLoading        = model.isLoading
                                item.openTargetDetails.connect(id_root.openTargetDetails)
                            }
                        }

                        Binding {
                            target: id_cardLoader.item
                            property: "p_isLoading"
                            value: model.isLoading
                            when: id_cardLoader.status === Loader.Ready
                        }
                    }
                }
            }
        }
    }
}
