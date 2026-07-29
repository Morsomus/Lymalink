/////////////////////////////////////////////////////////
// File: DashboardToolbar.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Unified toolbar for the Dashboard view.
//              Contains search, grid size selector, sort
//              dropdown, and filter chips.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0
import app.settings 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: id_root

    // Public ________________________________________________
    property bool p_targetDetailsVisible: false
    property bool p_addTargetVisible: false
    property bool p_targetHidden: false
    property int p_appId: 0
    property string p_targetType: ""
    property string p_toolbarTitle: ""
    property string p_activeLayout: "defaultCardGrid"
    property bool p_returnLocked: false

    signal layoutSelected(string size)
    signal returnClicked()
    signal missingMetadataReloadQueued(var targets)
    signal targetsVisibilityChanged()
    signal addTargetClicked()
    signal refreshClicked()
    signal reloadAssetsRequested(int appId, string targetType)
    signal targetDataUpdated(int appId, string targetType)
    signal targetHiddenChanged(int appId, string targetType, bool hidden)
    signal targetDeleted(int appId, string targetType)
    signal sortSelected(string sort)
    signal sortOrderSelected(bool descending)
    signal filtersSelected(var filters)
    signal searchTextChanged(string text)
    signal targetDetailsSortSelected(string sort)
    signal targetDetailsSortOrderSelected(bool descending)
    signal targetDetailsFiltersSelected(var filters)
    
    // Internals _____________________________________________
    property string activePanel: ""
    property string activeSort: ctxSettings.dashboardToolbarSort
    property string activeFilter: activeFilters.length > 1 ? "multiple" : activeFilters[0]
    property var activeFilters: ctxSettings.dashboardToolbarFilters.length > 0 ? ctxSettings.dashboardToolbarFilters : ["none"]
    readonly property int activeFilterCount: activeFilters.length
    property string targetDetailsActivePanel: ""
    property string targetDetailsActiveSort: "unlockDate"
    property string targetDetailsActiveFilter: "all"
    property var targetDetailsActiveFilters: ["all"]
    readonly property int targetDetailsActiveFilterCount: targetDetailsActiveFilters.length
    readonly property var targetDetailsSortModel: ["name", "unlockDate", "globalPercentage"]
    readonly property var targetDetailsFilterModel: ["all", "unlocked", "locked", "hidden"]
    readonly property var sortModel: ["title", "progress", "recentUnlock", "playtime", "lastPlayed", "dateAdded"]
    readonly property var filterModel: ["none", "completed", "uncompleted", "custom", "emulator", "steam", "hidden", "installed", "notInstalled"]
    readonly property color themedProgressColor: Themes.globalStyle.progressColor(ctxSettings.globalColorStyle)
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)
    readonly property var controlModel: [
        { value: "list", label: "List" },
        { value: "detailedList", label: "Details" },
        { value: "smallCardGrid", label: "Small" },
        { value: "defaultCardGrid", label: "Default" }
    ]

    implicitHeight: p_addTargetVisible
        ? id_addTargetToolbar.implicitHeight
        : p_targetDetailsVisible
            ? id_targetDetailsToolbar.implicitHeight
            : id_toolbar.implicitHeight

    Shortcut {
        sequence: "Esc"
        enabled: (id_root.p_targetDetailsVisible || id_root.p_addTargetVisible) && !id_targetSettingsPopup.opened && !id_root.p_returnLocked
        onActivated: {
            id_root.targetDetailsActivePanel = false
            id_root.returnClicked()
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: (id_root.p_targetDetailsVisible || id_root.p_addTargetVisible) && !id_targetSettingsPopup.opened && !id_root.p_returnLocked
        onActivated: {
            id_root.targetDetailsActivePanel = false
            id_root.returnClicked()
        }
    }

    Connections {
        target: ctxSettings

        function onSignalDefaultsReset() {
            id_root.activePanel = ""
            id_root.activeSort = ctxSettings.dashboardToolbarSort
            id_root.activeFilters = ctxSettings.dashboardToolbarFilters.length > 0 ? ctxSettings.dashboardToolbarFilters : ["none"]
            id_root.activeFilter = id_root.activeFilters.length > 1 ? "multiple" : id_root.activeFilters[0]
            id_orderPill.isDescending = ctxSettings.dashboardToolbarSortDescending
            id_root.targetDetailsActivePanel = ""
            id_root.targetDetailsActiveSort = "unlockDate"
            id_root.targetDetailsActiveFilters = ["all"]
            id_root.targetDetailsActiveFilter = "all"
            id_detailsOrderPill.isDescending = false
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
            case "steam":        return qsTr("Steam")
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
            case "globalPercentage": return qsTr("Global Percentage")
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
            id_root.targetDetailsFiltersSelected(filters)
        } else {
            activeFilters = filters
            activeFilter = filters.length > 1 ? "multiple" : filters[0]
            ctxSettings.SaveValue(Settings.DashboardToolbarFilters, filters)
            id_root.filtersSelected(filters)
        }
    }

    function toggleFilter(filter, toolbar) {
        const validToolbars = ["targetDetails", "default"]
        
        if (!validToolbars.includes(toolbar)) {
            console.error("DashboardToolbar - toggleFilter: Invalid toolbar param -", toolbar)
            return
        }

        const fallback = (toolbar === "default") ? "none" : "all"

        if (filter === fallback) {
            setActiveFilters([fallback], toolbar)
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

    TargetSettings {
        id: id_targetSettingsPopup

        parent: Overlay.overlay
        p_appId: id_root.p_appId
        p_targetType: id_root.p_targetType
        p_targetHidden: id_root.p_targetHidden
        onReloadAssetsRequested: function(appId, targetType) {
            id_root.reloadAssetsRequested(appId, targetType)
        }
        onTargetDataUpdated: function(appId, targetType) {
            id_root.targetDataUpdated(appId, targetType)
        }
        onTargetHiddenChanged: function(appId, targetType, hidden) {
            id_root.targetHiddenChanged(appId, targetType, hidden)
        }
        onTargetDeleted: function(appId, targetType) {
            id_root.targetDeleted(appId, targetType)
        }
    }

    DashboardSettings {
        id: id_dashboardSettingsPopup

        parent: Overlay.overlay
        onMissingMetadataReloadQueued: function(targets) {
            id_root.missingMetadataReloadQueued(targets)
        }
        onTargetsVisibilityChanged: {
            id_root.targetsVisibilityChanged()
        }
    }

    // Add New Target Toolbar
    ColumnLayout {
        id: id_addTargetToolbar

        visible: p_addTargetVisible
        anchors.fill: parent

        // Toolbar row: back image, wrapping title, toolbar column.
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            // Back arrow image
            Item {
                Layout.preferredWidth: id_addTargetBackArrowRow.implicitWidth
                Layout.preferredHeight: id_addTargetBackArrowRow.implicitHeight
                Layout.alignment: Qt.AlignTop

                MouseArea {
                    id: id_addTargetBackArrowMouseArea

                    anchors.fill: parent
                    enabled: p_addTargetVisible && !id_root.p_returnLocked
                    hoverEnabled: true
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        id_root.returnClicked()
                    }

                    RowLayout {
                        id: id_addTargetBackArrowRow

                        spacing: 10

                        CustomTooltip {
                            p_active: id_addTargetBackArrowMouseArea.containsMouse
                            p_delay: 500
                            p_text: qsTr("Backspace / Escape key")
                        }

                        Image {
                            id: id_addTargetBackArrowIcon

                            source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00033_ED.png"
                            Layout.preferredWidth: 35
                            Layout.preferredHeight: 35
                            rotation: 180
                            opacity: id_addTargetBackArrowMouseArea.containsMouse ? 0.7 : 1.0

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 120
                                }
                            }
                        }

                        Label {
                            id: id_addTargetTitleLabel

                            Layout.minimumWidth: 80
                            Layout.preferredWidth: Math.min(implicitWidth, 450)
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.title
                            font.bold: true
                            color: Themes.dashboardToolbar.colors.titleText
                            opacity: id_addTargetBackArrowMouseArea.containsMouse ? 0.7 : 1.0
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
        }
    }

    // Target Details Toolbar
    ColumnLayout {
        id: id_targetDetailsToolbar

        visible: p_targetDetailsVisible && !p_addTargetVisible
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

                        CustomTooltip {
                            p_active: id_backArrorIconMouseArea.containsMouse
                            p_delay: 500
                            p_text: qsTr("Backspace / Escape key")
                        }

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
            Rectangle {
                id: id_settingsIconPill

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                Layout.alignment: Qt.AlignTop

                implicitWidth: 32
                implicitHeight: 32
                radius: 16

                color: id_settingsIconMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillPressed
                    : id_settingsIconMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillHover
                        : Themes.dashboardToolbar.colors.pillBackground
                border.width: 1
                border.color: id_settingsIconMouseArea.pressed
                    ? Themes.dashboardToolbar.colors.pillBorderPressed
                    : id_settingsIconMouseArea.containsMouse
                        ? Themes.dashboardToolbar.colors.pillBorderHover
                        : Themes.dashboardToolbar.colors.pillBorder

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                Image {
                    id: id_settingsIcon

                    source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                    width: 24
                    height: 24
                    anchors.centerIn: parent
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    opacity: id_settingsIconMouseArea.containsMouse ? 0.78 : 1.0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                MouseArea {
                    id: id_settingsIconMouseArea

                    anchors.fill: parent
                    enabled: p_targetDetailsVisible
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        id_targetSettingsPopup.open()
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

                        property bool isDescending: false

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
                            onClicked: {
                                id_detailsOrderPill.isDescending = !id_detailsOrderPill.isDescending
                                id_root.targetDetailsSortOrderSelected(id_detailsOrderPill.isDescending)
                            }
                        }
                    }
                }

                // Details Sort/Filter chip bar
                Item {
                    implicitWidth: id_root.targetDetailsActivePanel === "detailsSort"
                        ? 320
                        : id_root.targetDetailsActivePanel === "detailsFilter"
                            ? 300
                            : 0
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
                            id_root.targetDetailsSortSelected(value)
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

            // Divider
            Rectangle {
                visible: id_root.p_targetType === "Emulator"
                width: 2
                Layout.fillHeight: true
                color: Themes.dashboardToolbar.colors.divider
                opacity: 0.5
            }

            // Refresh selected target
            Rectangle {
                id: id_detailsRefresh

                visible: id_root.p_targetType === "Emulator"
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignTop
                radius: 16

                property real refreshRotation: 0

                color: id_detailsRefreshMouseArea.pressed
                    ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.44)
                    : id_detailsRefreshMouseArea.containsMouse
                        ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.34)
                        : Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.00)

                border.width: 1
                border.color: id_detailsRefreshMouseArea.pressed
                    ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 1.00)
                    : id_detailsRefreshMouseArea.containsMouse
                        ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.92)
                        : Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.72)

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                Image {
                    id: id_detailsRefreshIcon

                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00038_ED.png"
                    width: 19
                    height: 19
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true

                    visible: false // MultiEffect draws image
                }

                MultiEffect {
                    anchors.fill: id_detailsRefreshIcon
                    source: id_detailsRefreshIcon
                    colorizationColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)
                    colorization: 1.0
                    rotation: id_detailsRefresh.refreshRotation
                }

                NumberAnimation {
                    id: id_detailsRefreshSpinAnimation

                    target: id_detailsRefresh
                    property: "refreshRotation"
                    from: 0
                    to: 360
                    duration: 300
                    easing.type: Easing.Linear
                }

                MouseArea {
                    id: id_detailsRefreshMouseArea

                    anchors.fill: parent
                    enabled: p_targetDetailsVisible
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        id_detailsRefreshSpinAnimation.restart()
                        id_root.refreshClicked()
                    }
                }
            }
        }
    }

    // Dashboard Toolbar
    ColumnLayout {
        id: id_toolbar

        visible: !p_targetDetailsVisible && !p_addTargetVisible
        anchors.fill: parent
        spacing: 8

        // Toolbar
        Item {
            id: id_toolbarLayout

            Layout.fillWidth: true
            implicitHeight: contentHeight

            property int spacing: 12
            property real contentHeight: 32

            function itemWidth(item) {
                return item.implicitWidth > 0 ? item.implicitWidth : item.width
            }

            function itemHeight(item) {
                return item.implicitHeight > 0 ? item.implicitHeight : item.height
            }

            function placeItem(item, itemX, itemY) {
                item.width = itemWidth(item)
                item.height = itemHeight(item)
                item.x = itemX
                item.y = itemY
            }

            function scheduleRelayout() {
                Qt.callLater(relayoutToolbar)
            }

            function relayoutToolbar() {
                const maxWidth = Math.max(0, width)
                const items = [
                    id_toolbarTitle,
                    id_searchGroup,
                    id_layoutGroup,
                    id_sortFilterGroup
                ]
                const actionItem = id_toolbarActionsGroup
                const actionWidth = itemWidth(actionItem)
                let firstRowWidth = 0
                let firstRowHeight = 0
                let splitIndex = 0

                for (let i = 0; i < items.length; ++i) {
                    const item = items[i]
                    const itemW = itemWidth(item)
                    const candidateWidth = splitIndex === 0
                        ? itemW
                        : firstRowWidth + spacing + itemW
                    const candidateWithActions = candidateWidth + spacing + actionWidth

                    if (candidateWithActions <= maxWidth || splitIndex === 0) {
                        firstRowWidth = candidateWidth
                        firstRowHeight = Math.max(firstRowHeight, itemHeight(item))
                        splitIndex = i + 1
                    } else {
                        break
                    }
                }

                let itemX = 0
                for (let i = 0; i < splitIndex; ++i) {
                    const item = items[i]
                    placeItem(item, itemX, 0)
                    itemX += itemWidth(item) + spacing
                }

                placeItem(actionItem, itemX, 0)
                firstRowHeight = Math.max(firstRowHeight, itemHeight(actionItem))

                itemX = 0
                let itemY = firstRowHeight + spacing
                let rowHeight = 0

                for (let i = splitIndex; i < items.length; ++i) {
                    const item = items[i]
                    const itemW = itemWidth(item)

                    if (itemX > 0 && itemX + spacing + itemW > maxWidth) {
                        itemX = 0
                        itemY += rowHeight + spacing
                        rowHeight = 0
                    }

                    placeItem(item, itemX, itemY)
                    itemX += itemW + spacing
                    rowHeight = Math.max(rowHeight, itemHeight(item))
                }

                contentHeight = splitIndex >= items.length
                    ? firstRowHeight
                    : itemY + rowHeight
            }

            Component.onCompleted: scheduleRelayout()
            onWidthChanged: scheduleRelayout()

            // Title
            Label {
                id: id_toolbarTitle

                text: id_root.p_toolbarTitle
                font.pixelSize: Themes.dashboardToolbar.fontSizes.title
                font.bold: true
                color: Themes.dashboardToolbar.colors.titleText
                onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()
            }

            // Divider + Search bar
            RowLayout {
                id: id_searchGroup

                spacing: id_toolbarLayout.spacing
                onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()

                Rectangle {
                    id: id_toolbarTitleDivider

                    width: 1
                    implicitHeight: 32
                    color: Themes.dashboardToolbar.colors.divider
                    opacity: 0.5
                }

                Rectangle {
                    id: id_dashboardSettings

                    implicitWidth: 32
                    implicitHeight: 32
                    radius: 16

                    color: id_dashboardSettingsMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillPressed
                        : id_dashboardSettingsMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillHover
                            : Themes.dashboardToolbar.colors.pillBackground
                    border.width: 1
                    border.color: id_dashboardSettingsMouseArea.pressed
                        ? Themes.dashboardToolbar.colors.pillBorderPressed
                        : id_dashboardSettingsMouseArea.containsMouse
                            ? Themes.dashboardToolbar.colors.pillBorderHover
                            : Themes.dashboardToolbar.colors.pillBorder

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    Image {
                        id: id_dashboardSettingsIcon

                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00004_ED.png"
                        width: 24
                        height: 24
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: id_dashboardSettingsMouseArea.containsMouse ? 0.78 : 1.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }

                    MouseArea {
                        id: id_dashboardSettingsMouseArea

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            id_dashboardSettingsPopup.open()
                        }
                    }
                }

                Rectangle {
                    id: id_searchBar

                    implicitWidth: 200
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

                        CustomTextField {
                            id: id_searchInput

                            Layout.fillWidth: true
                            placeholderText: qsTr("Search...")
                            color: Themes.dashboardToolbar.colors.searchText
                            font.pixelSize: Themes.dashboardToolbar.fontSizes.searchInput
                            verticalAlignment: TextInput.AlignVCenter
                            background: Item {}
                            onTextChanged: id_root.searchTextChanged(text)
                            Keys.onEscapePressed: {
                                text = ""
                                focus = false
                            }
                        }
                    }
                }
            }

            // Divider + Segmented layout control
            RowLayout {
                id: id_layoutGroup

                spacing: id_toolbarLayout.spacing
                onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()

                Rectangle {
                    id: id_layoutDivider

                    width: 1
                    implicitHeight: 32
                    color: Themes.dashboardToolbar.colors.divider
                    opacity: 0.5
                }

                // Segmented control: List | Details | Small | Default
                Rectangle {
                    id: id_layoutControl

                    implicitHeight: 32
                    implicitWidth: id_pillRow.implicitWidth + 4
                    radius: 16
                    color: Themes.dashboardToolbar.colors.pillBackground
                    border.width: 1
                    border.color: Themes.dashboardToolbar.colors.pillBorder
                    onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()

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

            // Divider + Filter + Sort + Order
            RowLayout {
                id: id_sortFilterGroup

                spacing: id_toolbarLayout.spacing
                onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()

                Rectangle {
                    id: id_searchDivider

                    width: 1
                    implicitHeight: 32
                    color: Themes.dashboardToolbar.colors.divider
                    opacity: 0.5
                }

                C_SortFilterPill {
                    id: id_filterPill

                    pillLabel: qsTr("Filter:")
                    isOpen: id_root.activePanel === "filter"
                    isValueActive: id_root.activeFilter !== ""
                    onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()
                    pillValue: {
                        if (id_root.activeFilterCount > 1) {
                            return qsTr("Multiple") + (" (%1)").arg(id_root.activeFilterCount)
                        }
                        return id_root.filterLabel(id_root.activeFilter)
                    }
                    onPillClicked: id_root.activePanel = id_filterPill.isOpen ? "" : "filter"
                }

                C_SortFilterPill {
                    id: id_sortPill

                    pillLabel: qsTr("Sort:")
                    isOpen: id_root.activePanel === "sort"
                    isValueActive: true
                    pillValue: id_root.sortLabel(id_root.activeSort)
                    onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()
                    onPillClicked: id_root.activePanel = id_sortPill.isOpen ? "" : "sort"
                }

                // Order pill
                // \u2193 Downward arrow
                // \u2191 Upward arrow
                Rectangle {
                    id: id_orderPill

                    property bool isDescending: ctxSettings.dashboardToolbarSortDescending

                    implicitHeight: 32
                    implicitWidth: id_orderRow.implicitWidth + 20
                    radius: 16
                    onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()
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
                        onClicked: {
                            id_orderPill.isDescending = !id_orderPill.isDescending
                            ctxSettings.SaveValue(Settings.DashboardToolbarSortDescending, id_orderPill.isDescending)
                            id_root.sortOrderSelected(id_orderPill.isDescending)
                        }
                    }
                }
            }

            // Add Target & Refresh button
            Item {
                id: id_toolbarActionsGroup

                implicitWidth: id_toolbarActionsOuterRow.implicitWidth
                implicitHeight: id_toolbarActionsOuterRow.implicitHeight
                onImplicitWidthChanged: id_toolbarLayout.scheduleRelayout()

                RowLayout {
                    id: id_toolbarActionsOuterRow

                    anchors.fill: parent
                    spacing: 12

                    // Divider
                    Rectangle {
                        width: 1
                        implicitHeight: 32
                        color: Themes.dashboardToolbar.colors.divider
                        opacity: 0.5
                    }

                    RowLayout {
                        id: id_toolbarActionsRow

                        spacing: 8

                        // Add Target Button
                        Rectangle {
                            id: id_addTarget

                            implicitHeight: 32
                            implicitWidth: id_addTargetLabel.implicitWidth + 24
                            radius: 16

                            color: id_addTargetMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.44)
                                : id_addTargetMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.34)
                                    : Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.00)

                            border.width: 1
                            border.color: id_addTargetMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 1.00)
                                : id_addTargetMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.92)
                                    : Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.72)

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Text {
                                id: id_addTargetLabel

                                anchors.centerIn: parent
                                text: qsTr("Add Target")
                                color: id_root.themedCompletionColor
                                font.pixelSize: Themes.dashboardToolbar.fontSizes.pillLabel
                            }

                            MouseArea {
                                id: id_addTargetMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: addTargetClicked()
                            }
                        }

                        // Refresh Button
                        Rectangle {
                            id: id_refresh

                            implicitWidth: 32
                            implicitHeight: 32
                            width: 32
                            height: 32
                            radius: 16

                            property real refreshRotation: 0

                            color: id_refreshMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.44)
                                : id_refreshMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.34)
                                    : Themes.globalStyle.withAlpha(id_root.themedProgressColor, 0.00)

                            border.width: 1
                            border.color: id_refreshMouseArea.pressed
                                ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 1.00)
                                : id_refreshMouseArea.containsMouse
                                    ? Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.92)
                                    : Themes.globalStyle.withAlpha(id_root.themedCompletionColor, 0.72)

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }

                            Image {
                                id: id_refreshIcon

                                anchors.centerIn: parent
                                source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00038_ED.png"
                                width: 19
                                height: 19
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true

                                visible: false // MultiEffect draws image
                            }

                            // Colorize refresh icon dynamically based on color theme
                            MultiEffect {
                                anchors.fill: id_refreshIcon
                                source: id_refreshIcon
                                colorizationColor: Themes.globalStyle.completionColor(ctxSettings.globalColorStyle)
                                colorization: 1.0
                                rotation: id_refresh.refreshRotation
                            }

                            NumberAnimation {
                                id: id_refreshSpinAnimation

                                target: id_refresh
                                property: "refreshRotation"
                                from: 0
                                to: 360
                                duration: 300
                                easing.type: Easing.Linear
                            }

                            MouseArea {
                                id: id_refreshMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    id_refreshSpinAnimation.restart()
                                    id_root.refreshClicked()
                                }
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
                    ctxSettings.SaveValue(Settings.DashboardToolbarSort, value)
                    id_root.sortSelected(value)
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
