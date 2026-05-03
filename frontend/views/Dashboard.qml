/////////////////////////////////////////////////////////
// File: Dashboard.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Dashboard displaying tracked content.
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import app.themes 1.0

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        anchors.topMargin: 48
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Label {
                Layout.fillWidth: false
                text: qsTr("Dashboard")
                font.pixelSize: Themes.dashboard.fontSizes.title
                font.bold: true
                color: Themes.dashboard.colors.titleText
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Themes.dashboard.colors.divider
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            maximumLineCount: 3
            wrapMode: Text.WordWrap
            text: "Tracked content will be added here."
            font.pixelSize: Themes.dashboard.fontSizes.body
            color: Themes.dashboard.colors.bodyText
        }

        Item { Layout.fillHeight: true }
    }
}
