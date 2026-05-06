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
    property string activeFilter: "None"
    property string activeSort: "Last Played"

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
                property bool hovered: false
                property bool pressed: false

                implicitHeight: 32
                implicitWidth: filterRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : hovered
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : hovered
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
                        text: root.activeFilter !== "" ? qsTr(root.activeFilter) : qsTr("Filter")
                        color: root.activeFilter !== "" ? Themes.dashboardToolbar.colors.pillValueActive : Themes.dashboardToolbar.colors.pillLabel
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: filterPill.hovered = true
                    onExited: filterPill.hovered = false
                    onPressed: filterPill.pressed = true
                    onReleased: filterPill.pressed = false
                    onCanceled: filterPill.pressed = false
                    onClicked: root.activePanel = filterPill.isOpen ? "" : "filter"
                }
            }

            // Sort pill
            Rectangle {
                id: sortPill

                readonly property bool isOpen: root.activePanel === "sort"
                property bool hovered: false
                property bool pressed: false

                implicitHeight: 32
                implicitWidth: sortRow.implicitWidth + 20
                radius: 16
                color: isOpen
                    ? Themes.dashboardToolbar.colors.pillOpen
                    : pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : hovered
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: isOpen
                    ? Themes.dashboardToolbar.colors.pillBorderOpen
                    : pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : hovered
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
                        text: qsTr(root.activeSort)
                        color: Themes.dashboardToolbar.colors.pillValueActive
                        font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: sortPill.hovered = true
                    onExited: sortPill.hovered = false
                    onPressed: sortPill.pressed = true
                    onReleased: sortPill.pressed = false
                    onCanceled: sortPill.pressed = false
                    onClicked: root.activePanel = sortPill.isOpen ? "" : "sort"
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
                            property bool hovered: false
                            property bool pressed: false
                            implicitWidth: pillLabel.implicitWidth + 20
                            implicitHeight: 26
                            radius: 13
                            color: active
                                ? Themes.dashboardToolbar.colors.segmentActive
                                : pressed
                                    ? Themes.dashboardToolbar.colors.segmentPressed
                                    : hovered
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
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onPressed: parent.pressed = true
                                onReleased: parent.pressed = false
                                onCanceled: parent.pressed = false
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
                        duration: 140
                    }
                }

                Label {
                    text: qsTr("Sort by:")
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsLabel
                    color: Themes.dashboardToolbar.colors.chipsLabel
                }

                Repeater {
                    model: [qsTr("Last Played"), qsTr("Title"), qsTr("Date Added"), qsTr("Playtime")]
                    delegate: Rectangle {
                        readonly property bool active: modelData === root.activeSort
                        property bool hovered: false
                        property bool pressed: false
                        implicitHeight: 26
                        implicitWidth: sortChipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : hovered
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : hovered
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
                            text: modelData
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
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                            onPressed: parent.pressed = true
                            onReleased: parent.pressed = false
                            onCanceled: parent.pressed = false
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
                        duration: 140
                    }
                }

                Label {
                    text: qsTr("Filter by:")
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsLabel
                    color: Themes.dashboardToolbar.colors.chipsLabel
                }

                Repeater {
                    model: [qsTr("None"), qsTr("Installed"), qsTr("Not Installed"), qsTr("Hidden"), qsTr("Completed"), qsTr("Uncompleted")]
                    delegate: Rectangle {
                        readonly property bool active: modelData === root.activeFilter
                        property bool hovered: false
                        property bool pressed: false
                        implicitHeight: 26
                        implicitWidth: chipLabel.implicitWidth + 20
                        radius: 13
                        color: active
                            ? Themes.dashboardToolbar.colors.chipsActive
                            : pressed
                                ? Themes.dashboardToolbar.colors.chipsPressed
                                : hovered
                                    ? Themes.dashboardToolbar.colors.chipsHover
                                    : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: active
                            ? Themes.dashboardToolbar.colors.chipsBorderActive
                            : pressed
                                ? Themes.dashboardToolbar.colors.chipsBorderPressed
                                : hovered
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
                            text: modelData
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
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.hovered = true
                            onExited: parent.hovered = false
                            onPressed: parent.pressed = true
                            onReleased: parent.pressed = false
                            onCanceled: parent.pressed = false
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
