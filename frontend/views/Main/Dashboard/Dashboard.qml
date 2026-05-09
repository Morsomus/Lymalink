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

            Image {
                anchors.centerIn: parent
                width:  Math.min(parent.width, parent.height) * 0.75
                height: width
                source: "qrc:/resources/images/empty_state.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: 0.35
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.verticalCenter
                anchors.topMargin: 16
                spacing: 6

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Your collection is empty.")
                    font.pixelSize: Themes.dashboard.fontSizes.emptyTitle
                    font.bold: true
                    color: Themes.dashboard.colors.titleText
                    opacity: 0.7
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Start by scanning a directory in Settings.")
                    font.pixelSize: Themes.dashboard.fontSizes.emptyBody
                    color: Themes.dashboard.colors.bodyText
                    opacity: 0.5
                }
            }
        }

        // Main content: CardGrid or CardList
        Loader {
            id: id_cardLayoutLoader

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !id_root.isEmpty
            sourceComponent: id_root.activeGridSize === "list" ? id_cardListLayout : id_cardGridLayout

            onLoaded: {
                if (id_root.activeGridSize !== "list") {
                    item.gridSize = id_root.activeGridSize
                } 
            }
        }

        Binding {
            when: id_root.activeGridSize !== "list"
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
        
        CardList {}
    }
}
