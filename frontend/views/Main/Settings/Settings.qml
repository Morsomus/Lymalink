/////////////////////////////////////////////////////////
// File: Main.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Settings page.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        anchors.topMargin: 48
        spacing: 18

        Label {
            Layout.fillWidth: true
            text: qsTr("Settings")
            font.pixelSize: Themes.settings.fontSizes.title
            font.bold: true
            color: Themes.settings.colors.titleText
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Themes.settings.colors.divider
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 680
            columns: 2
            columnSpacing: 18
            rowSpacing: 14

            Label {
                text: qsTr("Theme")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("System"), qsTr("Dark"), qsTr("Light")]
                currentIndex: 1

                delegate: ItemDelegate {
                    width: parent.width
                    text: modelData
                    enabled: index !== 0 && index !== 2 // TODO: Disabled until developed
                }
            }

            Label {
                text: qsTr("Language")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("English")]
            }

            Label {
                text: qsTr("Minimize to tray")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Tooltips")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Installation not found icon")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Total achievements badge")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Hide Lymalink Logo")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: false
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
