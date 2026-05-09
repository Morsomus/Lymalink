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

// TODO
// - Planning
// - Tooltips

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
                model: ["system", "dark", "light"]
                currentIndex: 1
                displayText: model[currentIndex] === "system"
                    ? qsTr("System")
                    : model[currentIndex] === "dark"
                        ? qsTr("Dark")
                        : qsTr("Light")
                delegate: ItemDelegate {
                    width: parent.width
                    text: {
                        switch (modelData) {
                            case "system": return qsTr("System")
                            case "dark":   return qsTr("Dark")
                            case "light":  return qsTr("Light")
                        }
                    }
                    enabled: modelData !== "system" && modelData !== "light" // TODO: Disabled until developed
                }
            }

            Label {
                text: qsTr("Language")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            ComboBox {
                Layout.fillWidth: true
                model: ["English", "Suomi", "Svenska"]
                currentIndex: 0
                displayText: model[currentIndex]
                delegate: ItemDelegate {
                    width: parent.width
                    text: modelData
                    enabled: modelData !== "Suomi" && modelData !== "Svenska" // TODO: Disabled until developed
                }
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
                text: qsTr("Progress frame")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Progress frame gray mode")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: false
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Progress frame completion animation")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Lymalink Logo")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Collapse button")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: false
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }

            Label {
                text: qsTr("Collapse border")
                color: Themes.settings.colors.labelText
                font.pixelSize: Themes.settings.fontSizes.label
            }
            Switch {
                checked: true
                text: checked ? qsTr("Enabled") : qsTr("Disabled")
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
