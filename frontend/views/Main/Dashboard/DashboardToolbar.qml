/////////////////////////////////////////////////////////
// File: DashboardToolbar.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Unified toolbar for the Dashboard view.
//              Contains search, grid size selector, sort
//              dropdown, and filter chips.
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import app.themes 1.0

Item {
    id: root

    property string activeGridSize: "default"
    property string activePanel: ""
    property string activeFilter: "none"
    property string activeSort: "title"

    signal gridSizeSelected(string size)

    implicitHeight: toolbarCol.implicitHeight

    ColumnLayout {
        id: toolbarCol

        anchors {
            left: parent.left
            right: parent.right
        }
        spacing: 8

        // Toolbar
        Flow {
            Layout.fillWidth: true
            spacing: 12

            // Title
            Label {
                text: qsTr("Dashboard")
                font.pixelSize: Themes.dashboardToolbar.fontSizes.title
                font.bold: true
                color: Themes.dashboardToolbar.colors.titleText
            }

            // Divider
            Rectangle {
                width: 1
                implicitHeight: 32
                color: Themes.dashboardToolbar.colors.divider
                opacity: 0.5
            }

            // Search bar
            Rectangle {
                implicitWidth: 220
                implicitHeight: 32
                radius: 16
                color: Themes.dashboardToolbar.colors.searchBackground
                border.width: 1
                border.color: Themes.dashboardToolbar.colors.searchBorder

                RowLayout {
                    anchors {
                        fill: parent
                        leftMargin: 10
                        rightMargin: 10
                    }
                    spacing: 6

                    Text {
                        text: "⌕"
                        color: Themes.dashboardToolbar.colors.searchIcon
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.searchIcon
                    }

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search...")
                        color: Themes.dashboardToolbar.colors.searchText
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.searchInput
                        verticalAlignment: TextField.AlignVCenter
                        background: Item {}
                        Keys.onEscapePressed: {
                            text = ""
                            focus = false
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                width: 1
                implicitHeight: 32
                color: Themes.dashboardToolbar.colors.divider
                opacity: 0.5
            }

            // Filter pill
            Rectangle {
                id: filterPill

                readonly property bool isOpen: root.activePanel === "filter"

                implicitHeight: 32
                implicitWidth: filterRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : filterPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : filterPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : filterPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : filterPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: filterRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Filter:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }

                    Text {
                        text: {
                            switch (root.activeFilter) {
                                case "none":         return qsTr("None")
                                case "installed":    return qsTr("Installed")
                                case "notInstalled": return qsTr("Not Installed")
                                case "hidden":       return qsTr("Hidden")
                                case "completed":    return qsTr("Completed")
                                case "uncompleted":  return qsTr("Uncompleted")
                                default:             return qsTr("Error")
                            }
                        }
                        color: root.activeFilter !== "" ? Themes.dashboardToolbar.colors.pillValueActive : Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }
                }

                MouseArea {
                    id: filterPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.activePanel = filterPill.isOpen ? "" : "filter"
                }
            }

            // Sort pill
            Rectangle {
                id: sortPill

                readonly property bool isOpen: root.activePanel === "sort"

                implicitHeight: 32
                implicitWidth: sortRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : sortPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : sortPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : sortPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : sortPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: sortRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Sort:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }
                    Text {
                        text: {
                            switch (root.activeSort) {
                                case "lastPlayed": return qsTr("Last Played")
                                case "title":      return qsTr("Title")
                                case "dateAdded":  return qsTr("Date Added")
                                case "playtime":   return qsTr("Playtime")
                                default:           return qsTr("Error")
                            }
                        }
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }
                }

                MouseArea {
                    id: sortPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.activePanel = sortPill.isOpen ? "" : "sort"
                }
            }

            // Order pill
            // \u2193 Downward arrow
            // \u2191 Upward arrow
            Rectangle {
                id: orderPill

                property bool isDescending: true

                implicitHeight: 32
                implicitWidth: orderRow.implicitWidth + 20
                radius: 16
                color: orderPillMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillPressed
                    : orderPillMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillHover
                        : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: orderPillMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillBorderPressed
                    : orderPillMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillBorderHover
                        : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: orderRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Order:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }
                    Text {
                        id: arrowIcon
                        text: "\u2191"
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillOrderValue
                        
                        // Transform
                        rotation: orderPill.isDescending ? 180 : 0
                        
                        Behavior on rotation {
                            RotationAnimation {
                                duration: 200
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }

                MouseArea {
                    id: orderPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: orderPill.isDescending = !orderPill.isDescending
                }
            }

            // Divider
            Rectangle {
                width: 1
                implicitHeight: 32
                color: Themes.dashboardToolbar.colors.divider
                opacity: 0.5
            }

            // Segmented control: List | Small | Default
            Rectangle {
                implicitHeight: 32
                implicitWidth: pillRow.implicitWidth + 4
                radius: 16
                color: Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: Themes.dashboardToolbar.colors.pillBorder

                RowLayout {
                    id: pillRow

                    anchors.centerIn: parent
                    spacing: 2

                    Repeater {
                        model: ["list", "small", "default"]
                        delegate: Rectangle {
                            readonly property bool active: modelData === root.activeGridSize
                            implicitWidth: pillLabel.implicitWidth + 20
                            implicitHeight: 26
                            radius: 13
                            color: active
                                ? Themes.dashboardToolbar.colors.segmentActive
                                : pillMouseArea.pressed
                                    ? Themes.dashboardToolbar.colors.segmentPressed
                                    : pillMouseArea.containsMouse
                                        ? Themes.dashboardToolbar.colors.segmentHover
                                        : Themes.dashboardToolbar.colors.segmentBackground

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Text {
                                id: pillLabel

                                anchors.centerIn: parent
                                text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                                color: active ? Themes.dashboardToolbar.colors.segmentLabelActive : Themes.dashboardToolbar.colors.segmentLabel
                                font.pixelSize: Themes.dashboardToolbar.fontSizes.segmentLabel
                                font.bold: active

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                            }

                            MouseArea {
                                id: pillMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.gridSizeSelected(modelData)
                            }
                        }
                    }
                }
            }
        }

        // Sort/Filter selection bar
        Item {
            Layout.fillWidth: true
            implicitHeight: root.activePanel === "sort" 
                ? sortBar.implicitHeight
                : root.activePanel === "filter"
                    ? filterBar.implicitHeight
                    : 0
            clip: true

            Behavior on implicitHeight {
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutCubic
                }
            }

            // Sort selection
            Flow {
                id: sortBar

                width: parent.width
                spacing: 8
                visible: root.activePanel === "sort"
                y: root.activePanel === "sort" ? 0 : -12
                opacity: root.activePanel === "sort" ? 1 : 0

                Behavior on y {
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Label {
                    text: qsTr("Sort by:")
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsLabel
                    color: Themes.dashboardToolbar.colors.chipsLabel
                }

                Repeater {
                    model: ["lastPlayed", "title", "dateAdded", "playtime"]
                    delegate: Rectangle {
                        readonly property bool active: modelData === root.activeSort
                        implicitHeight: 26
                        implicitWidth: sortChipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : sortBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : sortBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : sortBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : sortBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsBorderHover
                                    : Themes.dashboardToolbar.colors.pillBorder

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }

                        Text {
                            id: sortChipLabel

                            anchors.centerIn: parent
                            text: {
                                switch (modelData) {
                                    case "lastPlayed": return qsTr("Last Played")
                                    case "title":      return qsTr("Title")
                                    case "dateAdded":  return qsTr("Date Added")
                                    case "playtime":   return qsTr("Playtime")
                                    default:           return modelData
                                }
                            }
                            color: active ? Themes.dashboardToolbar.colors.chipsTextActive : Themes.dashboardToolbar.colors.chipsText
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsText
                            font.bold: active

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }

                        MouseArea {
                            id: sortBarMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                root.activeSort = modelData
                                root.activePanel = ""
                            }
                        }
                    }
                }
            }

            // Filter selection
            Flow {
                id: filterBar

                width: parent.width
                spacing: 8
                visible: root.activePanel === "filter"
                y: root.activePanel === "filter" ? 0 : -12
                opacity: root.activePanel === "filter" ? 1 : 0

                Behavior on y {
                    NumberAnimation {
                        duration: 180
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Label {
                    text: qsTr("Filter by:")
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsLabel
                    color: Themes.dashboardToolbar.colors.chipsLabel
                }

                Repeater {
                    model: ["none", "installed", "notInstalled", "hidden", "completed", "uncompleted"]
                    delegate: Rectangle {
                        readonly property bool active: modelData === root.activeFilter
                        implicitHeight: 26
                        implicitWidth: chipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : filterBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : filterBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : filterBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : filterBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsBorderHover
                                    : Themes.dashboardToolbar.colors.pillBorder

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }

                        Text {
                            id: chipLabel

                            anchors.centerIn: parent
                            text: {
                                switch (modelData) {
                                    case "none":         return qsTr("None")
                                    case "installed":    return qsTr("Installed")
                                    case "notInstalled": return qsTr("Not Installed")
                                    case "hidden":       return qsTr("Hidden")
                                    case "completed":    return qsTr("Completed")
                                    case "uncompleted":  return qsTr("Uncompleted")
                                    default:             return modelData
                                }
                            }
                            color: active ? Themes.dashboardToolbar.colors.chipsTextActive : Themes.dashboardToolbar.colors.chipsText
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsText
                            font.bold: active
                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }

                        MouseArea {
                            id: filterBarMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                root.activeFilter = modelData
                                root.activePanel = ""
                            }
                        }
                    }
                }
            }
        }
    }
}
