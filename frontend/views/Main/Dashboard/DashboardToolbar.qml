/////////////////////////////////////////////////////////
// File: DashboardToolbar.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Unified toolbar for the Dashboard view.
//              Contains search, grid size selector, sort
//              dropdown, and filter chips.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: id_root

    // Public ________________________________________________
    property bool p_targetDetailsVisible: false
    property string p_toolbarTitle: ""
    property string p_activeLayout: "defaultCardGrid"

    signal layoutSelected(string size)
    signal returnClicked()
    
    // Internals _____________________________________________
    property string activePanel: ""
    property string activeSort: "title"
    property string activeFilter: "none"
    property var activeFilters: ["none"]
    readonly property int activeFilterCount: activeFilters.length
    property string targetDetailsActivePanel: ""
    property string targetDetailsActiveSort: "name"
    property string targetDetailsActiveFilter: "all"
    property var targetDetailsActiveFilters: ["all"]
    readonly property int targetDetailsActiveFilterCount: targetDetailsActiveFilters.length
    readonly property var targetDetailsSortModel: ["name", "unlockDate"]
    readonly property var targetDetailsFilterModel: ["all", "unlocked", "locked", "hidden"]
    readonly property var sortModel: ["title", "progress", "recentUnlock", "playtime", "lastPlayed", "dateAdded"]
    readonly property var filterModel: ["none", "completed", "uncompleted", "custom", "emulator", "official", "hidden", "installed", "notInstalled"]
    readonly property var controlModel: [
        { value: "list", label: "List" },
        { value: "detailedList", label: "Details" },
        { value: "smallCardGrid", label: "Small" },
        { value: "defaultCardGrid", label: "Default" }
    ]

    implicitHeight: !p_targetDetailsVisible ? id_toolbar.implicitHeight : id_targetDetailsToolbar.implicitHeight

    Shortcut {
        sequence: "Esc"
        enabled: id_root.p_targetDetailsVisible
        onActivated: {
            id_root.targetDetailsActivePanel = false
            id_root.returnClicked()
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.p_targetDetailsVisible
        onActivated: {
            id_root.targetDetailsActivePanel = false
            id_root.returnClicked()
        }
    }

    function sortLabel(sort) {
        switch (sort) {
            case "title":        return qsTr("Title")
            case "progress":     return qsTr("Progress")
            case "recentUnlock": return qsTr("Recent Unlock")
            case "playtime":     return qsTr("Playtime")
            case "lastPlayed":   return qsTr("Last Played")
            case "dateAdded":    return qsTr("Date Added")
            default:             return qsTr("Error")
        }
    }

    function filterLabel(filter) {
        switch (filter) {
            case "none":         return qsTr("None")
            case "completed":    return qsTr("Completed")
            case "uncompleted":  return qsTr("Uncompleted")
            case "custom":       return qsTr("Custom")
            case "emulator":     return qsTr("Emulator")
            case "official":     return qsTr("Official")
            case "hidden":       return qsTr("Hidden")
            case "installed":    return qsTr("Installed")
            case "notInstalled": return qsTr("Not Installed")
            default:             return qsTr("Error")
        }
    }

    function targetDetailsSortLabel(sort) {
        switch (sort) {
            case "name":       return qsTr("Name")
            case "unlockDate": return qsTr("Unlock Date")
            default:           return qsTr("Error")
        }
    }

    function targetDetailsFilterLabel(filter) {
        switch (filter) {
            case "all":      return qsTr("All")
            case "unlocked": return qsTr("Unlocked")
            case "locked":   return qsTr("Locked")
            case "hidden":   return qsTr("Hidden")
            default:         return qsTr("Error")
        }
    }

    function hasFilter(filter, toolbar) {
        const selectedToolbar = toolbar === "targetDetails" ? "targetDetails" : "default"

        if (selectedToolbar === "targetDetails") {
            return targetDetailsActiveFilters.indexOf(filter) !== -1
        } else {
            return activeFilters.indexOf(filter) !== -1
        }
    }

    function setActiveFilters(filters, toolbar) {
        const selectedToolbar = toolbar === "targetDetails" ? "targetDetails" : "default"

        if (selectedToolbar === "targetDetails") {
            targetDetailsActiveFilters = filters
            targetDetailsActiveFilter = filters.length > 1 ? "multiple" : filters[0]
        } else {
            activeFilters = filters
            activeFilter = filters.length > 1 ? "multiple" : filters[0]
        }
    }

    function toggleFilter(filter, toolbar) {
        const validToolbars = ["targetDetails", "default"]
        
        if (!validToolbars.includes(toolbar)) {
            console.error("DashboardToolbar - toggleFilter: Invalid toolbar param -", toolbar)
            return
        }

        if (filter === "none") {
            setActiveFilters(["none"], toolbar)
            return
        }

        let currentFilters = (toolbar === "default")
            ? activeFilters.filter(item => item !== "none")
            : targetDetailsActiveFilters.filter(item => item !== "all")

        const index = currentFilters.indexOf(filter)
        if (index === -1) {
            currentFilters.push(filter)
        } else {
            currentFilters.splice(index, 1)
        }

        const fallback = (toolbar === "default") ? "none" : "all"
        setActiveFilters(currentFilters.length > 0 ? currentFilters : [fallback], toolbar)
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Component - A pill button used for Filter, Sort, and Order controls.
    component C_SortFilterPill: Rectangle {
        id: id_pill

        property string pillLabel: ""       // e.g. "Filter:", "Sort:", "Order:"
        property string pillValue: ""       // displayed value text
        property bool isOpen: false         // drives the open colour state
        property bool isValueActive: true   // false keeps value in label colour

        signal pillClicked()

        implicitHeight: 32
        implicitWidth: id_pillInnerRow.implicitWidth + 20
        radius: 16

        color: isOpen
            ? Themes.dashboardToolbar.colors.pillOpen
            : id_pillMouseArea.pressed
                ? Themes.dashboardToolbar.colors.pillPressed
                : id_pillMouseArea.containsMouse
                    ? Themes.dashboardToolbar.colors.pillHover
                    : Themes.dashboardToolbar.colors.pillBackground

        border.width: 1
        border.color: isOpen
            ? Themes.dashboardToolbar.colors.pillBorderOpen
            : id_pillMouseArea.pressed
                ? Themes.dashboardToolbar.colors.pillBorderPressed
                : id_pillMouseArea.containsMouse
                    ? Themes.dashboardToolbar.colors.pillBorderHover
                    : Themes.dashboardToolbar.colors.pillBorder

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }

        RowLayout {
            id: id_pillInnerRow

            anchors.centerIn: parent
            spacing: 6

            Text {
                text: id_pill.pillLabel
                color: Themes.dashboardToolbar.colors.pillLabel
                font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
            }

            Text {
                text: id_pill.pillValue
                color: id_pill.isValueActive
                    ? Themes.dashboardToolbar.colors.pillValueActive
                    : Themes.dashboardToolbar.colors.pillLabel
                font.pixelSize: Themes.dashboardToolbar.fontSizes.pillValue
            }
        }

        MouseArea {
            id: id_pillMouseArea

            anchors.fill: parent
            hoverEnabled: true
            onClicked: id_pill.pillClicked()
        }
    }

    // Component - A chip row used for Sort-by and Filter-by selection bars.
    component C_ChipBar: Flow {
        id: id_chipBar

        property string barLabel: ""                            // e.g. "Sort by:", "Filter by:"
        property var chipModel: []                              // array of value strings
        property var labelFn: function(v) { return v }          // value → display string
        property var isActiveFn: function(v) { return false }   // value → bool

        signal chipClicked(string value)

        spacing: 8

        Label {
            text: id_chipBar.barLabel
            font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsLabel
            color: Themes.dashboardToolbar.colors.chipsLabel
        }

        Repeater {
            model: id_chipBar.chipModel
            delegate: Rectangle {
                readonly property bool active: id_chipBar.isActiveFn(modelData)

                implicitHeight: 26
                implicitWidth: id_chipLabel.implicitWidth + 20
                radius: 13

                color: active
                    ? Themes.dashboardToolbar.colors.chipsActive
                    : id_chipMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.chipsPressed
                        : id_chipMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.chipsHover
                            : Themes.dashboardToolbar.colors.pillBackground

                border.width: 1
                border.color: active
                    ? Themes.dashboardToolbar.colors.chipsBorderActive
                    : id_chipMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.chipsBorderPressed
                        : id_chipMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.chipsBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                Text {
                    id: id_chipLabel

                    anchors.centerIn: parent
                    text: id_chipBar.labelFn(modelData)
                    color: active
                        ? Themes.dashboardToolbar.colors.chipsTextActive
                        : Themes.dashboardToolbar.colors.chipsText
                    font.pixelSize: Themes.dashboardToolbar.fontSizes.chipsText
                    font.bold: active

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }

                MouseArea {
                    id: id_chipMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: id_chipBar.chipClicked(modelData)
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Target Details Toolbar
    ColumnLayout {
        id: id_targetDetailsToolbar

        visible: p_targetDetailsVisible
        anchors.fill: parent

        // Toolbar row: back image, wrapping title, toolbar column.
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            // Back arrow image
            Item {
                Layout.preferredWidth: id_backArrowRow.implicitWidth
                Layout.preferredHeight: id_backArrowRow.implicitHeight
                Layout.alignment: Qt.AlignTop

                MouseArea {
                    id: id_backArrorIconMouseArea

                    anchors.fill: parent
                    enabled: p_targetDetailsVisible
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        id_root.targetDetailsActivePanel = false
                        id_root.returnClicked()
                    }

                    RowLayout {
                        id: id_backArrowRow

                        spacing: 10

                        ToolTip.visible: id_backArrorIconMouseArea.containsMouse
                        ToolTip.text: qsTr("Backspace / Escape key")
                        ToolTip.delay: 500

                        Image {
                            id: id_backArrowIcon

                            source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00033_ED.png"
                            Layout.preferredWidth: 35
                            Layout.preferredHeight: 35
                            rotation: 180
                            opacity: id_backArrorIconMouseArea.containsMouse ? 0.7 : 1.0

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 120
                                }
                            }
                        }

                        Label {
                            id: id_titleLabel

                            Layout.minimumWidth: 80
                            Layout.preferredWidth: Math.min(implicitWidth, 450)
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.title
                            font.bold: true
                            color: Themes.dashboardToolbar.colors.titleText
                            opacity: id_backArrorIconMouseArea.containsMouse ? 0.7 : 1.0
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            text: id_root.p_toolbarTitle

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 120
                                }
                            }
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                width: 2
                Layout.fillHeight: true
                color: Themes.dashboardToolbar.colors.divider
                opacity: 0.5
            }

            // Settings Icon for Selected Target
            Item {
                Layout.preferredWidth: id_settingsIcon.width
                Layout.preferredHeight: id_settingsIcon.height
                Layout.alignment: Qt.AlignTop

                MouseArea {
                    id: id_settingsIconMouseArea

                    anchors.fill: parent
                    enabled: p_targetDetailsVisible
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        // TODO: Open Selected Target Settings
                    }

                    Image {
                        id: id_settingsIcon

                        source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                        width: 32
                        height: 32
                        anchors.centerIn: parent
                        opacity: id_settingsIconMouseArea.containsMouse ? 0.7 : 1.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }
                }
            }

            // Target Details Toolbar controls
            ColumnLayout {
                id: id_detailsToolbarColumn

                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: id_detailsToolbarFlow.implicitWidth
                spacing: 8

                Flow {
                    id: id_detailsToolbarFlow

                    spacing: 12

                    // Filter pill
                    C_SortFilterPill {
                        id: id_detailsFilterPill

                        pillLabel: qsTr("Filter:")
                        isOpen: id_root.targetDetailsActivePanel === "detailsFilter"
                        isValueActive: id_root.targetDetailsActiveFilter !== ""
                        pillValue: {
                            if (id_root.targetDetailsActiveFilterCount > 1) {
                                return qsTr("Multiple") + (" (%1)").arg(id_root.targetDetailsActiveFilterCount)
                            }
                            return id_root.targetDetailsFilterLabel(id_root.targetDetailsActiveFilter)
                        }
                        onPillClicked: id_root.targetDetailsActivePanel = id_detailsFilterPill.isOpen ? "" : "detailsFilter"
                    }

                    // Sort pill
                    C_SortFilterPill {
                        id: id_detailsSortPill

                        pillLabel: qsTr("Sort:")
                        isOpen: id_root.targetDetailsActivePanel === "detailsSort"
                        isValueActive: true
                        pillValue: id_root.targetDetailsSortLabel(id_root.targetDetailsActiveSort)
                        onPillClicked: id_root.targetDetailsActivePanel = id_detailsSortPill.isOpen ? "" : "detailsSort"
                    }

                    // Order pill
                    Rectangle {
                        id: id_detailsOrderPill

                        property bool isDescending: true

                        implicitHeight: 32
                        implicitWidth: id_detailsOrderRow.implicitWidth + 20
                        radius: 16
                        color: id_detailsOrderMouseArea.pressed
                            ? Themes.dashboardToolbar.colors.pillPressed
                            : id_detailsOrderMouseArea.containsMouse
                                ? Themes.dashboardToolbar.colors.pillHover
                                : Themes.dashboardToolbar.colors.pillBackground
                        border.width: 1
                        border.color: id_detailsOrderMouseArea.pressed
                            ? Themes.dashboardToolbar.colors.pillBorderPressed
                            : id_detailsOrderMouseArea.containsMouse
                                ? Themes.dashboardToolbar.colors.pillBorderHover
                                : Themes.dashboardToolbar.colors.pillBorder

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }

                        RowLayout {
                            id: id_detailsOrderRow

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
                                rotation: id_detailsOrderPill.isDescending ? 180 : 0

                                Behavior on rotation {
                                    RotationAnimation {
                                        duration: 200
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: id_detailsOrderMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: id_detailsOrderPill.isDescending = !id_detailsOrderPill.isDescending
                        }
                    }
                }

                // Details Sort/Filter chip bar
                Item {
                    Layout.fillWidth: true
                    implicitHeight: id_root.targetDetailsActivePanel === "detailsSort"
                        ? id_detailsSortBar.implicitHeight
                        : id_root.targetDetailsActivePanel === "detailsFilter"
                            ? id_detailsFilterBar.implicitHeight
                            : 0

                    Behavior on implicitHeight {
                        NumberAnimation {
                            duration: 180
                            easing.type: Easing.OutCubic
                        }
                    }

                    // Sort chips
                    C_ChipBar {
                        id: id_detailsSortBar

                        width: parent.width
                        barLabel: qsTr("Sort by:")
                        chipModel: id_root.targetDetailsSortModel
                        labelFn: id_root.targetDetailsSortLabel
                        isActiveFn: function(v) { return v === id_root.targetDetailsActiveSort }
                        visible: id_root.targetDetailsActivePanel === "detailsSort"
                        opacity: id_root.targetDetailsActivePanel === "detailsSort" ? 1 : 0
                        onChipClicked: function(value) {
                            id_root.targetDetailsActiveSort  = value
                            id_root.targetDetailsActivePanel = ""
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 150
                            }
                        }
                    }

                    // Filter chips
                    C_ChipBar {
                        id: id_detailsFilterBar

                        width: parent.width
                        barLabel: qsTr("Filter by:")
                        chipModel: id_root.targetDetailsFilterModel
                        labelFn: id_root.targetDetailsFilterLabel
                        isActiveFn: function(v) { return id_root.hasFilter(v, "targetDetails") }
                        visible: id_root.targetDetailsActivePanel === "detailsFilter"
                        opacity: id_root.targetDetailsActivePanel === "detailsFilter" ? 1 : 0
                        onChipClicked: function(value) {
                            id_root.toggleFilter(value, "targetDetails")
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 150
                            }
                        }
                    }
                }
            }
        }
    }

    // Dashboard Toolbar
    ColumnLayout {
        id: id_toolbar

        visible: !p_targetDetailsVisible
        anchors.fill: parent
        spacing: 8

        // Toolbar
        Flow {
            Layout.fillWidth: true
            spacing: 12

            // Title
            Label {
                text: id_root.p_toolbarTitle
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
            C_SortFilterPill {
                id: id_filterPill

                pillLabel: qsTr("Filter:")
                isOpen: id_root.activePanel === "filter"
                isValueActive: id_root.activeFilter !== ""
                pillValue: {
                    if (id_root.activeFilterCount > 1) {
                        return qsTr("Multiple") + (" (%1)").arg(id_root.activeFilterCount)
                    }
                    return id_root.filterLabel(id_root.activeFilter)
                }
                onPillClicked: id_root.activePanel = id_filterPill.isOpen ? "" : "filter"
            }

            // Sort pill
            C_SortFilterPill {
                id: id_sortPill

                pillLabel: qsTr("Sort:")
                isOpen: id_root.activePanel === "sort"
                isValueActive: true
                pillValue: id_root.sortLabel(id_root.activeSort)
                onPillClicked: id_root.activePanel = id_sortPill.isOpen ? "" : "sort"
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

            // Segmented control: List | Details | Small | Default
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
                        model: id_root.controlModel
                        delegate: Rectangle {
                            readonly property bool active: modelData.value === id_root.p_activeLayout
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
                                text: modelData.label
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
                                onClicked: id_root.layoutSelected(modelData.value)
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

            Behavior on implicitHeight {
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutCubic
                }
            }

            // Sort selection
            C_ChipBar {
                id: id_sortBar

                width: parent.width
                barLabel: qsTr("Sort by:")
                chipModel: id_root.sortModel
                labelFn: id_root.sortLabel
                isActiveFn: function(v) { return v === id_root.activeSort }
                visible: id_root.activePanel === "sort"
                opacity: id_root.activePanel === "sort" ? 1 : 0
                onChipClicked: function(value) {
                    id_root.activeSort  = value
                    id_root.activePanel = ""
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
            }

            // Filter selection
            C_ChipBar {
                id: id_filterBar

                width: parent.width
                barLabel: qsTr("Filter by:")
                chipModel: id_root.filterModel
                labelFn: id_root.filterLabel
                isActiveFn: function(v) { return id_root.hasFilter(v, "default") }
                visible: id_root.activePanel === "filter"
                opacity: id_root.activePanel === "filter" ? 1 : 0
                onChipClicked: function(value) {
                    id_root.toggleFilter(value, "default")
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }
            }
        }
    }
}