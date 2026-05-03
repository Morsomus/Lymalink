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

import Lymalink

ApplicationWindow {
    visible: true
    width: 1440
    height: 900
    title: qsTr("Lymalink")
    minimumWidth: 900
    minimumHeight: 600

    background: Rectangle {
        color: "#181818"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
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
                id: stackLayout
                anchors.fill: parent
                anchors.leftMargin: 18
                currentIndex: sidebar.currentPage

                Dashboard{}
                Settings{}
            }
        }
    }
}
