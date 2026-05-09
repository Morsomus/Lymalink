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
    id: id_root

    property string activeGridSize: "default"
    property string activePanel: ""
    property string activeFilter: "none"
    property string activeSort: "title"

    signal gridSizeSelected(string size)

    implicitHeight: id_toolbar.implicitHeight

    ColumnLayout {
        id: id_toolbar

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
                id: id_filterPill

                readonly property bool isOpen: id_root.activePanel === "filter"

                implicitHeight: 32
                implicitWidth: id_filterRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : id_filterPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : id_filterPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : id_filterPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : id_filterPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: id_filterRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Filter:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }

                    Text {
                        text: {
                            switch (id_root.activeFilter) {
                                case "none":         return qsTr("None")
                                case "installed":    return qsTr("Installed")
                                case "notInstalled": return qsTr("Not Installed")
                                case "hidden":       return qsTr("Hidden")
                                case "completed":    return qsTr("Completed")
                                case "uncompleted":  return qsTr("Uncompleted")
                                default:             return qsTr("Error")
                            }
                        }
                        color: id_root.activeFilter !== "" ? Themes.dashboardToolbar.colors.pillValueActive : Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }
                }

                MouseArea {
                    id: id_filterPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: id_root.activePanel = id_filterPill.isOpen ? "" : "filter"
                }
            }

            // Sort pill
            Rectangle {
                id: id_sortPill

                readonly property bool isOpen: id_root.activePanel === "sort"

                implicitHeight: 32
                implicitWidth: id_sortRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : id_sortPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : id_sortPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : id_sortPillMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : id_sortPillMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: id_sortRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Sort:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }
                    Text {
                        text: {
                            switch (id_root.activeSort) {
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
                    id: id_sortPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: id_root.activePanel = id_sortPill.isOpen ? "" : "sort"
                }
            }

            // Order pill
            // \u2193 Downward arrow
            // \u2191 Upward arrow
            Rectangle {
                id: id_orderPill

                property bool isDescending: true

                implicitHeight: 32
                implicitWidth: id_orderRow.implicitWidth + 20
                radius: 16
                color: id_orderPillMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillPressed
                    : id_orderPillMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillHover
                        : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: id_orderPillMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillBorderPressed
                    : id_orderPillMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillBorderHover
                        : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    id: id_orderRow

                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: qsTr("Order:")
                        color: Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                    }
                    Text {
                        text: "\u2191"
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillOrderValue
                        
                        // Transform
                        rotation: id_orderPill.isDescending ? 180 : 0
                        
                        Behavior on rotation {
                            RotationAnimation {
                                duration: 200
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }

                MouseArea {
                    id: id_orderPillMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: id_orderPill.isDescending = !id_orderPill.isDescending
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
                implicitWidth: id_pillRow.implicitWidth + 4
                radius: 16
                color: Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: Themes.dashboardToolbar.colors.pillBorder

                RowLayout {
                    id: id_pillRow

                    anchors.centerIn: parent
                    spacing: 2

                    Repeater {
                        model: ["list", "small", "default"]
                        delegate: Rectangle {
                            readonly property bool active: modelData === id_root.activeGridSize
                            implicitWidth: id_pillLabel.implicitWidth + 20
                            implicitHeight: 26
                            radius: 13
                            color: active
                                ? Themes.dashboardToolbar.colors.segmentActive
                                : id_pillMouseArea.pressed
                                    ? Themes.dashboardToolbar.colors.segmentPressed
                                    : id_pillMouseArea.containsMouse
                                        ? Themes.dashboardToolbar.colors.segmentHover
                                        : Themes.dashboardToolbar.colors.segmentBackground

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Text {
                                id: id_pillLabel

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
                                id: id_pillMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: id_root.gridSizeSelected(modelData)
                            }
                        }
                    }
                }
            }
        }

        // Sort/Filter selection bar
        Item {
            Layout.fillWidth: true
            implicitHeight: id_root.activePanel === "sort" 
                ? id_sortBar.implicitHeight
                : id_root.activePanel === "filter"
                    ? id_filterBar.implicitHeight
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
                id: id_sortBar

                width: parent.width
                spacing: 8
                visible: id_root.activePanel === "sort"
                y: id_root.activePanel === "sort" ? 0 : -12
                opacity: id_root.activePanel === "sort" ? 1 : 0

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
                        readonly property bool active: modelData === id_root.activeSort
                        implicitHeight: 26
                        implicitWidth: id_sortChipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : id_sortBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : id_sortBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : id_sortBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : id_sortBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsBorderHover
                                    : Themes.dashboardToolbar.colors.pillBorder

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }

                        Text {
                            id: id_sortChipLabel

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
                            id: id_sortBarMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                id_root.activeSort = modelData
                                id_root.activePanel = ""
                            }
                        }
                    }
                }
            }

            // Filter selection
            Flow {
                id: id_filterBar

                width: parent.width
                spacing: 8
                visible: id_root.activePanel === "filter"
                y: id_root.activePanel === "filter" ? 0 : -12
                opacity: id_root.activePanel === "filter" ? 1 : 0

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
                        readonly property bool active: modelData === id_root.activeFilter
                        implicitHeight: 26
                        implicitWidth: id_filterChipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : id_filterBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : id_filterBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : id_filterBarMouseArea.pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : id_filterBarMouseArea.containsMouse
                                    ? Themes.dashboardToolbar.colors.chipsBorderHover
                                    : Themes.dashboardToolbar.colors.pillBorder

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }

                        Text {
                            id: id_filterChipLabel

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
                            id: id_filterBarMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                id_root.activeFilter = modelData
                                id_root.activePanel = ""
                            }
                        }
                    }
                }
            }
        }
    }
}
