/////////////////////////////////////////////////////////
// File: TargetDetails.qml
// Date: 2026-05-12
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Detail view for a tracked target. 
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
    property int p_appId: 0
    property string p_title: "Aethelwald III"
    property string p_coverSource: ""
    property string p_lastPlayed: ""
    property string p_recentUnlock: ""
    property string p_playtime: "" // minutes or hours
    property string p_targetType: ""
    property string p_installationStatus: ""
    property string p_emulatorType: ""
    property bool p_appIdDirFound: false
    property int p_achievementDataStatus: 0
    property int p_achievementCount: 0
    property int p_achievementTotal: 0
    property int p_globalColorStyle: 1
    property alias p_achievementModel: id_achievementList.model

    signal achievementStateChanged(int appId)
    signal backgroundClicked()

    // Internals _____________________________________________
    readonly property int coverPanelWidth: 240
    readonly property int coverHeight: 360
    readonly property real fixedPanelClearance: id_coverColumn.implicitHeight
    readonly property real fixedPanelInset: id_root.coverPanelWidth + 24
    readonly property bool hasVerticalScroll: id_achievementList.ScrollBar.vertical.size < 1.0
    readonly property bool p_enabledAchievementRowDynamicWidth: ctxSettings.enableDynamicAchievementRows
    readonly property color themedProgressColor: Themes.globalStyle.progressColor(p_globalColorStyle)
    readonly property color themedCompletionColor: Themes.globalStyle.completionColor(p_globalColorStyle)
    // Completion ratio used to drive the progress bar gradient and opacity
    readonly property real completionRatio: p_achievementTotal > 0
        ? p_achievementCount / p_achievementTotal
        : 0.0
    readonly property string achievementDataState: p_achievementDataStatus <= 0
        ? "missing"
        : p_achievementDataStatus === 1
            ? "initial"
            : "found"
    readonly property color achievementDataColor: achievementDataState === "found"
        ? Themes.targetDetails.colors.text
        : achievementDataState === "initial"
            ? Themes.targetDetails.colors.warningText
            : Themes.targetDetails.colors.errorText

    function emulatorLabel(emulatorType) {
        switch ((emulatorType || "").trim().toUpperCase()) {
        case "GOG-N":
            return "GOG-Nemirtingas"
        case "GOLDBERG":
            return "Goldberg"
        case "CODEX":
            return "Codex"
        case "RUNE":
            return "Rune"
        case "RLD":
            return "Reloaded"
        case "SMARTSTEAMEMU":
            return "SmartSteamEmu"
        case "TENOKE":
            return "Tenoke"
        default:
            return "-"
        }
    }

    function achievementDataLabel() {
        switch (achievementDataState) {
        case "found":
            return qsTr("Found")
        case "initial":
            return qsTr("Initial")
        default:
            return qsTr("Missing")
        }
    }

    function achievementDataTooltip(emulatorType) {
        if (achievementDataState === "found") {
            return ""
        }
        if (achievementDataState === "initial") {
            return qsTr("Initial achievement data was found, but no unlocked achievements have been detected yet. If achievement data does not update after an unlock, the game may not support achievements in this emulator.")
        }

        switch ((emulatorType || "").trim().toUpperCase()) {
        default:
            return qsTr("Some emulators create the initial achievement data when the game first starts; others create it after the first achievement unlocks.")
        }
    }

    // Component.onCompleted: {
    //     if (ctxSettings.targetDetailsHelpText !== LYMALINK_APP_VERSION) {
    //         id_targetHelpTextMarkdownPopup.openDocument(qsTr("Tips"), CREDITS_MD_TEXT)
    //     }
    // }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Component - Label + optional icon, value row used in the meta panel
    component C_MetaRow: RowLayout {
        property string icon:  ""
        property string label: ""
        property string value: ""
        property string tooltip: ""
        property bool showInfoMarker: tooltip !== ""
        property color valueColor: Themes.targetDetails.colors.text

        width: parent.width
        spacing: 6

        Text {
            visible: icon !== ""
            text: icon
            font.pixelSize: Themes.targetDetails.fontSizes.metaIcon
        }
        Text {
            text: label + ":  "
            color: Themes.targetDetails.colors.text
            font.pixelSize: Themes.targetDetails.fontSizes.metaLabel
            opacity: 0.55
        }
        Item {
            Layout.fillWidth: true
        }
        
        // Info text
        Rectangle {
            visible: showInfoMarker && tooltip !== ""
            width: 16
            height: 16
            radius: 8
            color: "transparent"
            border.width: 1
            border.color: Themes.targetDetails.colors.text
            opacity: 0.55

            Text {
                anchors.centerIn: parent
                text: "i"
                color: Themes.targetDetails.colors.text
                font.pixelSize: 11
                font.italic: true
                font.bold: true
            }

            HoverHandler {
                id: id_infoHover
                cursorShape: Qt.PointingHandCursor
            }

            CustomTooltip {
                p_active: id_infoHover.hovered
                p_alwaysVisible: true
                p_delay: 600
                p_text: tooltip
                p_maxLineCount: 5
            }
        }

        Text {
            text: value
            color: valueColor
            font.pixelSize: Themes.targetDetails.fontSizes.metaValue
            font.bold: true
            Layout.alignment: Qt.AlignRight
            opacity: 0.55
        }
    }

    // Component - Section divider shown between "Achievements" / "Locked" / "Hidden" groups
    component C_SectionHeader: Item {
        id: id_sectionHeader

        property real leftInset: 0
        property bool leftInsetAnimationEnabled: false

        width: ListView.view.width
        height: 36

        Component.onCompleted: Qt.callLater(function() {
            id_sectionHeader.leftInsetAnimationEnabled = true
        })

        Behavior on leftInset {
            enabled: id_sectionHeader.leftInsetAnimationEnabled
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutQuad
            }
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                leftMargin: id_sectionHeader.leftInset
                rightMargin: hasVerticalScroll ? 50 : 0
            }
            height: 1
            color: Themes.targetDetails.colors.divider
        }

        Text {
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
                leftMargin: id_sectionHeader.leftInset + 12
            }
            text: section === "unlocked"
                ? qsTr("Achievements")
                : section === "locked"
                    ? qsTr("Locked")
                    : qsTr("Hidden")
            color: Themes.targetDetails.colors.sectionHeaderText
            font.pixelSize: Themes.targetDetails.fontSizes.sectionTitle
            font.bold: true
            opacity: Themes.isLight ? 1.0 : 0.65
        }
    }

    // Component - Achievement Row
    component C_AchievementRow: Item {
        id: id_row

        property alias iconSource: id_icon.source
        property string achievementKey: ""
        property string achievementName: ""
        property string achievementDescription: ""
        property real globalUnlockPercentage: 0.0
        property int curProgress: 0
        property int maxProgress: 0
        property string unlockDate: ""
        property string unlockTime: ""
        property bool unlocked: false
        property bool achievementHidden: false
        property bool revealed: false

        // leftInset: positive = content shifted right, used for cover-zone indent
        property real leftInset: 0
        readonly property bool concealedHidden: achievementHidden && !unlocked
        readonly property bool contentRevealed: !concealedHidden || revealed
        readonly property real achievementProgressRatio: maxProgress > 0
            ? Math.max(0.0, Math.min(1.0, curProgress / maxProgress))
            : 0.0
        property real contentRevealOpacity: 0.0
        property real contentRevealOffset: 8.0
        property real hiddenPlaceholderOpacity: 0.0
        property bool leftInsetAnimationEnabled: false

        height: 82

        function snapToHidden() {
            id_contentRevealAnimation.stop()
            contentRevealOpacity     = 0.0
            contentRevealOffset      = 8.0
            hiddenPlaceholderOpacity = concealedHidden ? 0.35 : 0.0
        }

        function snapToRevealed() {
            id_contentRevealAnimation.stop()
            contentRevealOpacity     = 1.0
            contentRevealOffset      = 0.0
            hiddenPlaceholderOpacity = 0.0
        }

        function animateReveal() {
            id_contentRevealAnimation.stop()
            contentRevealOpacity     = 0.0
            contentRevealOffset      = 8.0
            hiddenPlaceholderOpacity = 0.35
            id_contentRevealAnimation.start()
        }

        onRevealedChanged: concealedHidden && (revealed ? animateReveal() : snapToHidden())
        onAchievementHiddenChanged: contentRevealed ? snapToRevealed() : snapToHidden()
        onUnlockedChanged: contentRevealed ? snapToRevealed() : snapToHidden()
        Component.onCompleted: {
            contentRevealed ? snapToRevealed() : snapToHidden()
            Qt.callLater(function() {
                id_row.leftInsetAnimationEnabled = true
            })
        }

        SequentialAnimation {
            id: id_contentRevealAnimation

            ParallelAnimation {
                NumberAnimation {
                    target: id_row
                    property: "contentRevealOpacity"
                    to: 1.0
                    duration: 520
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: id_row
                    property: "contentRevealOffset"
                    to: 0.0
                    duration: 520
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: id_row
                    property: "hiddenPlaceholderOpacity"
                    to: 0.0
                    duration: 260
                    easing.type: Easing.OutQuad
                }
            }
        }

        Behavior on leftInset {
            enabled: id_row.leftInsetAnimationEnabled
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutQuad
            }
        }

        TapHandler {
            enabled: id_row.achievementHidden && !id_row.unlocked
            onTapped: id_row.revealed = !id_row.revealed
            cursorShape: Qt.PointingHandCursor
        }

        HoverHandler {
            id: id_hoverHandler

            enabled: id_row.achievementHidden && !id_row.unlocked
            cursorShape: Qt.PointingHandCursor
        }

        RowLayout {
            anchors {
                fill: parent
                leftMargin: id_row.leftInset
                rightMargin: hasVerticalScroll ? 50 : 16
                topMargin: 6
                bottomMargin: 8
            }
            spacing: 14

            // Achievement icon
            Rectangle {
                width: 64
                height: 64
                color: id_row.concealedHidden && id_row.revealed
                    ? "transparent"
                    : Themes.targetDetails.colors.coverBackground

                Image {
                    id: id_icon

                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    visible: id_row.contentRevealOpacity > 0
                    opacity: id_row.contentRevealOpacity
                    transform: Translate { y: id_row.contentRevealOffset }
                }

                Text {
                    anchors.centerIn: parent
                    visible: id_row.concealedHidden && !id_row.revealed
                    text: "?"
                    color: Themes.targetDetails.colors.text
                    font.pixelSize: Themes.targetDetails.fontSizes.hiddenIcon
                    font.bold: true
                    opacity: id_row.hiddenPlaceholderOpacity
                }

                Item {
                    id: id_achievementEditOverlay

                    anchors.fill: parent
                    z: 10
                    visible: id_root.p_targetType !== "Steam" && id_root.p_appId > 0 && id_row.achievementKey.length > 0 && id_row.contentRevealOpacity > 0.95

                    Rectangle {
                        anchors.fill: parent
                        color: id_row.unlocked
                            ? id_root.themedCompletionColor
                            : id_root.themedProgressColor
                        opacity: id_achievementEditMouseArea.containsMouse ? 0.20 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: parent.width * 0.7
                        height: parent.height * 0.7
                        source: id_row.unlocked
                            ? "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00040_ED.png"
                            : "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00039_ED.png"
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        opacity: id_achievementEditMouseArea.containsMouse ? 0.65 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }

                    MouseArea {
                        id: id_achievementEditMouseArea

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton
                        onClicked: {
                            id_achievementEditPopup.configure(
                                id_root.p_appId,
                                id_row.achievementKey,
                                id_row.achievementName,
                                id_row.achievementDescription,
                                id_row.unlocked
                            )
                            id_achievementEditPopup.open()
                        }
                    }
                }

                TargetAchievementEditPopup {
                    id: id_achievementEditPopup

                    onConfirmed: function(appId, achievementKey, unlock, unlockTimestamp) {
                        if (ctxLymalink.SetAchievementUnlocked(appId, achievementKey, unlock, unlockTimestamp)) {
                            id_root.achievementStateChanged(appId)
                        }
                    }
                }
            }

            // Name + description
            Item {
                Layout.fillWidth: true
                implicitHeight: id_nameDescCol.implicitHeight

                Rectangle {
                    anchors.fill: parent
                    color: Themes.targetDetails.colors.hiddenHoverOverlay
                    opacity: id_hoverHandler.hovered ? 0.06 : 0.0
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                Column {
                    id: id_nameDescCol

                    width: parent.width
                    spacing: 4

                    // Fixed-height wrapper prevents Column reflow during the name cross-fade
                    Item {
                        width: parent.width
                        height: id_realNameText.implicitHeight

                        Text {
                            width: parent.width
                            text: qsTr("Hidden")
                            color: Themes.targetDetails.colors.text
                            font.pixelSize: Themes.targetDetails.fontSizes.rowName
                            font.bold: true
                            elide: Text.ElideRight
                            visible: id_row.hiddenPlaceholderOpacity > 0
                            opacity: id_row.hiddenPlaceholderOpacity
                        }

                        Text {
                            id: id_realNameText

                            width: parent.width
                            text: id_row.achievementName
                            color: Themes.targetDetails.colors.text
                            font.pixelSize: Themes.targetDetails.fontSizes.rowName
                            font.bold: true
                            elide: Text.ElideRight
                            visible: id_row.contentRevealOpacity > 0
                            opacity: id_row.contentRevealOpacity * (id_row.unlocked ? 1.0 : 0.55)
                            transform: Translate { y: id_row.contentRevealOffset }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: id_row.contentRevealOpacity > 0
                        text: id_row.achievementDescription
                        color: Themes.targetDetails.colors.text
                        font.pixelSize: Themes.targetDetails.fontSizes.rowDescription
                        opacity: 0.50 * id_row.contentRevealOpacity
                        transform: Translate { y: id_row.contentRevealOffset }
                        elide: Text.ElideRight
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                    }
                }
            }

            Column {
                Layout.preferredWidth: 220
                
                spacing: 7
                opacity: id_row.contentRevealOpacity
                transform: Translate {
                    y: id_row.contentRevealOffset
                }

                // Achivement progress track
                Item {
                    width: id_unlockGlobalRow.implicitWidth
                    height: 18
                    visible: id_row.maxProgress > 1

                    readonly property color progressColor: id_row.unlocked
                        ? id_root.themedCompletionColor
                        : Themes.globalStyle.withAlpha(Themes.targetDetails.colors.text, 0.45)

                    Rectangle {
                        id: id_achievementProgressTrack

                        anchors {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                        }
                        height: 3
                        color: Themes.globalStyle.withAlpha(Themes.targetDetails.colors.text, 0.16)

                        Rectangle {
                            anchors {
                                left: parent.left
                                top: parent.top
                                bottom: parent.bottom
                            }
                            width: parent.width * id_row.achievementProgressRatio
                            color: parent.parent.progressColor
                        }
                    }

                    Text {
                        anchors {
                            left: parent.left
                            bottom: id_achievementProgressTrack.top
                            bottomMargin: 2
                        }
                        width: 76
                        text: id_row.curProgress
                        color: parent.progressColor
                        font.pixelSize: Themes.targetDetails.fontSizes.rowGlobalLabel
                        font.bold: id_row.unlocked
                        elide: Text.ElideRight
                        opacity: id_row.unlocked ? 1.0 : 0.70
                    }

                    Text {
                        anchors {
                            right: parent.right
                            bottom: id_achievementProgressTrack.top
                            bottomMargin: 2
                        }
                        width: 76
                        text: id_row.maxProgress
                        color: parent.progressColor
                        font.pixelSize: Themes.targetDetails.fontSizes.rowGlobalLabel
                        font.bold: id_row.unlocked
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideLeft
                        opacity: id_row.unlocked ? 1.0 : 0.70
                    }
                }

                // Global unlock %
                Row {
                    id: id_unlockGlobalRow

                    width: parent.width
                    spacing: 3

                    Text {
                        text: id_row.globalUnlockPercentage.toFixed(1) + "%"
                        color: Themes.targetDetails.colors.text
                        font.pixelSize: Themes.targetDetails.fontSizes.rowGlobalPercent
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.65
                    }
                    Text {
                        text: qsTr("of players have this achievement")
                        color: Themes.targetDetails.colors.text
                        font.pixelSize: Themes.targetDetails.fontSizes.rowGlobalLabel
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.35
                    }
                }
            }

            // Unlock date (or locked indicator)
            Text {
                Layout.preferredWidth: 70
                text: id_row.unlocked ? ( id_row.unlockTime + "\n" + id_row.unlockDate ) : qsTr("Locked")
                color: id_row.unlocked ? id_root.themedCompletionColor : Themes.targetDetails.colors.text
                font.pixelSize: Themes.targetDetails.fontSizes.rowUnlockDate
                font.bold: id_row.unlocked
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                opacity: id_row.unlocked ? 1.0 : 0.35
            }
        }

        // Bottom separator
        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: id_row.leftInset
                rightMargin: hasVerticalScroll ? 50 : 0
            }
            height: 1
            color: Themes.targetDetails.colors.divider
            opacity: 0.4
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    MarkdownDocumentPopup {
        id: id_targetHelpTextMarkdownPopup
        onClosed: {
            if (ctxSettings.targetDetailsHelpText !== LYMALINK_APP_VERSION) {
                ctxSettings.SaveValue(Settings.TargetDetailsHelpText, LYMALINK_APP_VERSION)
            }
        }
    }

    // Left fixed panel - cover + meta
    Item {
        z: 2
        anchors {
            top: parent.top
            left: parent.left
            bottom: parent.bottom
            topMargin: 28
        }
        width: id_root.coverPanelWidth

        Column {
            id: id_coverColumn

            width: parent.width
            spacing: 14

            // Cover image
            Rectangle {
                width: id_root.coverPanelWidth
                height: id_root.coverHeight
                color: Themes.targetDetails.colors.coverBackground
                clip: true

                layer.enabled: true
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: id_coverMask
                }

                Image {
                    id: id_coverImage

                    anchors.fill: parent
                    source: id_root.p_coverSource
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true

                    CustomBusyIndicator {
                        anchors.centerIn: parent
                        visible: p_running
                        p_indicatorSize: 64
                        p_running: id_coverImage.status === Image.Loading
                    }

                    ErrorImage {
                        anchors.centerIn: parent
                        p_size: 96
                        visible: id_coverImage.status === Image.Error
                    }
                }

                // Fallback title
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 16
                    visible: id_coverImage.status !== Image.Ready
                    text: id_root.p_title
                    color: Themes.targetDetails.colors.coverFallbackText
                    font.pixelSize: Themes.targetDetails.fontSizes.coverFallback
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Rectangle {
                id: id_coverMask

                width: id_root.coverPanelWidth
                height: id_root.coverHeight
                radius: 8
                visible: false
                layer.enabled: true
            }

            // Meta rows
            Column {
                width: parent.width
                spacing: 6

                // Achievement Progress Bar
                Item {
                    id: id_progressBar

                    readonly property int segmentCount: 25
                    readonly property int litSegments: {
                        if (id_root.p_achievementTotal <= 0) return 0
                        if (id_root.p_achievementCount <= 0) return 0
                        if (id_root.completionRatio >= 1.0) return segmentCount
                        const natural = Math.floor(id_root.completionRatio * segmentCount)
                        return Math.max(1, natural)
                    }

                    visible: id_root.p_achievementTotal > 0
                    width: parent.width
                    height: 26

                    // Outer border
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.width: 1
                        border.color: Themes.globalStyle.withAlpha(
                            id_root.completionRatio >= 1.0
                                ? id_root.themedCompletionColor
                                : id_root.themedProgressColor,
                            0.55
                        )
                        z: 2
                    }

                    // Segment row
                    Row {
                        anchors {
                            fill: parent
                            margins: 2
                        }
                        spacing: 1

                        Repeater {
                            model: id_progressBar.segmentCount

                            Rectangle {
                                required property int index
                                readonly property bool lit: index < id_progressBar.litSegments
                                readonly property real segmentRatio: id_progressBar.segmentCount > 1
                                    ? index / (id_progressBar.segmentCount - 1)
                                    : 0.0
                                readonly property color litColor: Themes.globalStyle.mixColor(
                                    id_root.themedProgressColor,
                                    id_root.themedCompletionColor,
                                    id_root.completionRatio * segmentRatio
                                )

                                width: (id_progressBar.width - (id_progressBar.segmentCount - 1) * 1 - 4) / id_progressBar.segmentCount
                                height: parent.height
                                topLeftRadius: index === 0 ? 4 : 0
                                bottomLeftRadius: index === 0 ? 4 : 0
                                topRightRadius: index === id_progressBar.segmentCount - 1 ? 4 : 0
                                bottomRightRadius: index === id_progressBar.segmentCount - 1 ? 4 : 0
                                color: lit
                                    ? Qt.rgba(litColor.r, litColor.g, litColor.b, 0.55 + 0.45 * id_root.completionRatio)
                                    : Qt.rgba(0.15, 0.15, 0.15, 0.40)

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 400
                                    }
                                }
                            }
                        }
                    }

                    // Centered count label
                    Text {
                        anchors.centerIn: parent
                        text: id_root.p_achievementCount + " / " + id_root.p_achievementTotal
                        color: "white"
                        font.pixelSize: Themes.targetDetails.fontSizes.progressBar
                        font.bold: true
                        z: 3

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.implicitWidth + 14
                            height: parent.implicitHeight + 4
                            radius: 4
                            color: Qt.rgba(0, 0, 0, 0.35)
                            z: -1
                        }
                    }
                }

                C_MetaRow {
                    label: qsTr("Status")
                    value: id_root.p_installationStatus
                    visible: id_root.p_targetType !== "Steam"
                }
                C_MetaRow {
                    label: qsTr("Type")
                    value: id_root.p_targetType
                    visible: id_root.p_targetType !== ""
                }
                C_MetaRow {
                    label: qsTr("")
                    value: id_root.emulatorLabel(id_root.p_emulatorType)
                    visible: id_root.p_targetType === "Emulator" && id_root.emulatorLabel(id_root.p_emulatorType) !== "-"
                }
                C_MetaRow {
                    label: qsTr("Playtime")
                    value: id_root.p_playtime === "" ? qsTr("Never") : id_root.p_playtime
                    // visible: id_root.p_playtime !== ""
                }
                C_MetaRow {
                    label: qsTr("Last played")
                    value: id_root.p_lastPlayed === "" ? qsTr("Never") : id_root.p_lastPlayed
                    // visible: id_root.p_lastPlayed !== ""
                }
                C_MetaRow {
                    label: qsTr("Recent unlock")
                    value: id_root.p_recentUnlock === "" ? qsTr("Never") : id_root.p_recentUnlock
                    visible: id_root.p_achievementTotal > 0
                }
                C_MetaRow {
                    label: qsTr("Achievement data")
                    tooltip: id_root.achievementDataTooltip(id_root.p_emulatorType)
                    showInfoMarker: id_root.achievementDataState !== "found"
                    value: id_root.achievementDataLabel()
                    valueColor: id_root.achievementDataColor
                    visible: id_root.p_targetType !== "Steam"
                }

                // Bottom separator
                Rectangle {
                    width: parent.width
                    height: 3
                    color: Themes.targetDetails.colors.divider
                    opacity: 0.8
                }
            }
        }
    }

    // Achievement list scrolls under the fixed panel
    // Delegates are inset while their viewport position intersects the fixed cover/meta column
    ListView {
        id: id_achievementList

        anchors {
            fill: parent

            rightMargin: Math.max(0, parent.width - 1152) // Width cap for wider window
            leftMargin: id_root.p_enabledAchievementRowDynamicWidth ? 0 : id_root.coverPanelWidth + 24
            topMargin: 28
            bottomMargin: 28
        }
        spacing: 1
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: id_root.backgroundClicked()
        }

        ScrollBar.vertical: CustomScrollBar {
            id: id_verticalScrollBar

            policy: ScrollBar.AsNeeded
        }

        // Anchor ScrollBar to the right after content loaded
        Component.onCompleted: {
            id_verticalScrollBar.parent = id_root
            id_verticalScrollBar.anchors.top = id_achievementList.top
            id_verticalScrollBar.anchors.bottom = id_achievementList.bottom
            id_verticalScrollBar.anchors.right = id_root.right
        }

        // Section header - unlocked, locked, hidden
        section.property: "sectionKey"
        section.criteria: ViewSection.FullString
        section.delegate: C_SectionHeader {
            readonly property real viewportTop: y - id_achievementList.contentY
            leftInset: id_root.p_enabledAchievementRowDynamicWidth && viewportTop < id_root.fixedPanelClearance ? id_root.fixedPanelInset : 0
        }

        delegate: C_AchievementRow {
            width: id_achievementList.width

            readonly property real viewportTop: y - id_achievementList.contentY
            leftInset: id_root.p_enabledAchievementRowDynamicWidth && viewportTop < id_root.fixedPanelClearance ? id_root.fixedPanelInset : 0

            achievementKey: model.achievementKey
            iconSource: model.iconSource
            achievementName: model.achievementName
            achievementDescription: model.achievementDescription
            globalUnlockPercentage: model.globalUnlockPercentage
            curProgress: model.curProgress
            maxProgress: model.maxProgress
            unlockDate: model.unlockDate
            unlockTime: model.unlockTime
            unlocked: model.unlocked

            achievementHidden: model.achievementHidden
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            visible: id_achievementList.count === 0
            text: qsTr("No achievements found.")
            color: Themes.targetDetails.colors.text
            opacity: 0.4
            font.pixelSize: Themes.targetDetails.fontSizes.emptyState
        }
    }
}
