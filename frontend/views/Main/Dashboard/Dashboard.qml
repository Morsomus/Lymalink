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

    property string activeGridSize: "default"
    property bool isEmpty: false
    readonly property int requiredWindowMinimumWidth: activeGridSize === "details" ? 1280 : 900

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        anchors.topMargin: 48
        spacing: 12

        DashboardToolbar {
            Layout.fillWidth: true
            activeGridSize: id_root.activeGridSize
            onGridSizeSelected: (size) => id_root.activeGridSize = size
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
            visible: id_root.isEmpty

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width, 520)
                spacing: 18

                CustomBusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    indicatorSize: 280
                    speed: 8400
                    running: id_root.isEmpty
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
            visible: !id_root.isEmpty
            sourceComponent: id_root.activeGridSize === "list" || id_root.activeGridSize === "details" ? id_cardListLayout : id_cardGridLayout

            onLoaded: {
                if (id_root.activeGridSize !== "list" && id_root.activeGridSize !== "details") {
                    item.gridSize = id_root.activeGridSize
                } 
            }
        }

        Binding {
            when: id_root.activeGridSize !== "list"
                && id_root.activeGridSize !== "details"
                && id_cardLayoutLoader.status === Loader.Ready
                && id_cardLayoutLoader.sourceComponent === id_cardGridLayout
            target: id_cardLayoutLoader.item
            property: "gridSize"
            value: id_root.activeGridSize
        }
    }

    Component {
        id: id_cardGridLayout

        CardGrid {
            gridSize: id_root.activeGridSize
        }
    }

    Component {
        id: id_cardListLayout
        
        CardList {
            listMode: id_root.activeGridSize
        }
    }
}
