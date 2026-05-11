/////////////////////////////////////////////////////////
// File: Main.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Entry point for the Lymalink application
//              frontend.
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: id_root

    readonly property int defaultMinimumWidth: 900

    visible: true
    width: 1510
    height: 900
    title: qsTr("Lymalink")
    minimumWidth: id_sidebar.currentPage === 0 ? id_dashboard.requiredWindowMinimumWidth : defaultMinimumWidth
    minimumHeight: 600

    onMinimumWidthChanged: {
        if (width < minimumWidth) {
            width = minimumWidth
        }
    }

    background: Rectangle {
        color: "#181818"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: id_sidebar
        }

        // Main content area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1f1f1f"

            // Subtle left divider from sidebar
            Rectangle {
                width: 1
                height: parent.height
                anchors.left: parent.left
                color: "#2a2a2a"
            }

            StackLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                currentIndex: id_sidebar.currentPage

                Dashboard{
                    id: id_dashboard
                }
                Settings{}
            }
        }
    }
}
