/////////////////////////////////////////////////////////
// File: Themes.qml
// Date: 2026-05-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Singleton themes for QML
/////////////////////////////////////////////////////////

pragma Singleton
import QtQuick

QtObject {
    // General
    readonly property QtObject general: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: "#202020"
            readonly property color backgroundHover: "#252525"
            readonly property color backgroundPressed: "#2a2a2a"
            readonly property color border: "#2d2d2d"
            readonly property color statusActive: "#47d17c"
            readonly property color statusInactive: "#777777"
            readonly property color titleText: "#e6e6e6"
            readonly property color bodyText: "#9b9b9b"
            readonly property color tooltipBackground: "#222222"
            readonly property color linkText: "#dcdcdc"
            readonly property color linkBackground: "transparent"
            readonly property color linkBackgroundHover: "#252525"
            readonly property color linkBackgroundPressed: "#151515"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int statusCollapsed: 18
            readonly property int statusExpanded: 14
            readonly property int title: 13
            readonly property int body: 11
            readonly property int tooltip: 12
            readonly property int link: 14
        }
    }

    // Shared backend service status indicator
    readonly property QtObject serviceIndicator: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color running: "#47d17c"
            readonly property color starting: "#f2c94c"
            readonly property color error: "#d35f5f"
        }

        readonly property QtObject opacity: QtObject {
            readonly property real solid: 1.0
            readonly property real breathingLow: 0.35
        }

        readonly property QtObject animation: QtObject {
            readonly property int breathingDuration: 900
        }
    }

    // ErrorPopup.qml
    readonly property QtObject errorPopup: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: "#242424"
            readonly property color border: "#d35f5f"
            readonly property color titleText: "#e6e6e6"
            readonly property color bodyText: "#9b9b9b"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 14
            readonly property int body: 12
        }
    }

    // Global style themes shared across application
    readonly property QtObject globalStyle: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color edgeFrameBack: Qt.rgba(0, 0, 0, 0.85)

            // Gray mode
            readonly property color edgeFrameGray: Qt.rgba(0.50, 0.50, 0.50, 0.70)

            // 0: Gold
            readonly property color gold_incomplete_hi: Qt.rgba(1.00, 0.80, 0.40, 0.85)
            readonly property color gold_doneStatic: Qt.rgba(1.00, 0.80, 0.40, 0.90)
            readonly property color edgeFrameGold_animA: Qt.rgba(0.70, 0.62, 0.15, 0.85)
            readonly property color edgeFrameGold_animB: Qt.rgba(1.00, 0.95, 0.55, 1.00)

            // 1: Electric Blue
            readonly property color blue_incomplete_hi: Qt.rgba(0.24, 0.42, 1.00, 0.85)
            readonly property color blue_doneStatic: Qt.rgba(0.30, 0.54, 1.00, 0.90)
            readonly property color edgeFrameBlue_animA: Qt.rgba(0.10, 0.33, 1.00, 0.85)
            readonly property color edgeFrameBlue_animB: Qt.rgba(0.53, 0.67, 1.00, 1.00)

            // 2: Purple
            readonly property color purple_incomplete_hi: Qt.rgba(0.48, 0.18, 1.00, 0.85)
            readonly property color purple_doneStatic: Qt.rgba(0.62, 0.36, 1.00, 0.90)
            readonly property color edgeFramePurple_animA: Qt.rgba(0.48, 0.18, 1.00, 0.85)
            readonly property color edgeFramePurple_animB: Qt.rgba(0.78, 0.60, 1.00, 1.00)

            // 3: Emerald
            readonly property color emerald_incomplete_hi: Qt.rgba(0.10, 0.48, 0.29, 0.85)
            readonly property color emerald_doneStatic: Qt.rgba(0.18, 0.74, 0.43, 0.90)
            readonly property color edgeFrameEmerald_animA: Qt.rgba(0.10, 0.60, 0.33, 0.85)
            readonly property color edgeFrameEmerald_animB: Qt.rgba(0.36, 1.00, 0.63, 1.00)

            // 4: Ember
            readonly property color ember_incomplete_hi: Qt.rgba(0.75, 0.20, 0.10, 0.85)
            readonly property color ember_doneStatic: Qt.rgba(1.00, 0.38, 0.19, 0.90)
            readonly property color edgeFrameEmber_animA: Qt.rgba(0.87, 0.25, 0.06, 0.85)
            readonly property color edgeFrameEmber_animB: Qt.rgba(1.00, 0.67, 0.27, 1.00)

            // 5: Frost
            readonly property color frost_incomplete_hi: Qt.rgba(1.00, 1.00, 1.00, 0.35)
            readonly property color frost_doneStatic: Qt.rgba(0.78, 0.90, 1.00, 0.75)
            readonly property color edgeFrameFrost_animA: Qt.rgba(0.78, 0.90, 1.00, 0.60)
            readonly property color edgeFrameFrost_animB: Qt.rgba(1.00, 1.00, 1.00, 0.90)
        }

        function progressColor(s) {
            if (s === 1) return colors.blue_incomplete_hi
            if (s === 2) return colors.purple_incomplete_hi
            if (s === 3) return colors.emerald_incomplete_hi
            if (s === 4) return colors.ember_incomplete_hi
            if (s === 5) return colors.frost_incomplete_hi
            return colors.gold_incomplete_hi
        }

        function completionColor(s) {
            if (s === 1) return colors.blue_doneStatic
            if (s === 2) return colors.purple_doneStatic
            if (s === 3) return colors.emerald_doneStatic
            if (s === 4) return colors.ember_doneStatic
            if (s === 5) return colors.frost_doneStatic
            return colors.gold_doneStatic
        }

        function edgeFrameAnimAColor(s) {
            if (s === 1) return colors.edgeFrameBlue_animA
            if (s === 2) return colors.edgeFramePurple_animA
            if (s === 3) return colors.edgeFrameEmerald_animA
            if (s === 4) return colors.edgeFrameEmber_animA
            if (s === 5) return colors.edgeFrameFrost_animA
            return colors.edgeFrameGold_animA
        }

        function edgeFrameAnimBColor(s) {
            if (s === 1) return colors.edgeFrameBlue_animB
            if (s === 2) return colors.edgeFramePurple_animB
            if (s === 3) return colors.edgeFrameEmerald_animB
            if (s === 4) return colors.edgeFrameEmber_animB
            if (s === 5) return colors.edgeFrameFrost_animB
            return colors.edgeFrameGold_animB
        }

        function withAlpha(c, a) {
            return Qt.rgba(c.r, c.g, c.b, a)
        }

        function mixColor(a, b, t) {
            t = Math.max(0.0, Math.min(t, 1.0))
            return Qt.rgba(
                a.r + (b.r - a.r) * t,
                a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t,
                a.a + (b.a - a.a) * t
            )
        }

        function progressBlendColor(s, t) {
            const grey = 0.45
            const p = Math.max(0.0, Math.min(t, 1.0))
            const hi = progressColor(s)
            return Qt.rgba(
                grey + p * (hi.r - grey),
                grey + p * (hi.g - grey),
                grey + p * (hi.b - grey),
                0.60 + p * 0.25
            )
        }
    }

    // Dashboard.qml
    readonly property QtObject dashboard: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color divider: "#2a2a2a"
            readonly property color bodyText: "#999999"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int body: 15
            readonly property int emptyTitle: 25
            readonly property int emptyBody: 13
        }
    }

    // Settings.qml
    readonly property QtObject settings: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color divider: "#2a2a2a"
            readonly property color subDivider: "#242424"
            readonly property color labelText: "#d8d8d8"
            readonly property color sectionTitle: "#666677"
            readonly property color sectionInfo: "#666677"
            readonly property color applyFlash: "#55cc88"
            readonly property color infoBoxBorder: "#34363a"
            readonly property color infoBoxBackground: "#202124"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int titleText: 28
            readonly property int labelText: 14
            readonly property int sectionTitle: 15
            readonly property int sectionInfo: 11
        }
    }

    // Sidebar.qml
    readonly property QtObject sidebar: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color panel: "#181818"
            readonly property color divider: "#2a2a2a"
            readonly property color versionText: "#666666"

            readonly property color collapseText: "#d0d0d0"
            readonly property color collapseBackground: "#202020"
            readonly property color collapseBackgroundHover: "#303030"
            readonly property color collapseBackgroundPressed: "#151515"
            readonly property color collapseBorder: "#343434"

            readonly property color navText: "#bdbdbd"
            readonly property color navTextSelected: "#ffffff"
            readonly property color navBackground: "transparent"
            readonly property color navBackgroundHover: "#252525"
            readonly property color navBackgroundSelected: "#001b74"
            readonly property color navBackgroundPressed: "#151515"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int version: 11
            readonly property int collapseButton: 12
            readonly property int navIcon: 16
            readonly property int navLabel: 16
        }
    }

    // SidebarButton.qml
    readonly property QtObject sidebarButton: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color navText: "#bdbdbd"
            readonly property color navBackground: "transparent"
        }
        readonly property QtObject fontSizes: QtObject {
            readonly property int navIcon: 16
            readonly property int navLabel: 16
        }
    }

    // ErrorImage.qml
    readonly property QtObject errorImage: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color errorImageText: "#b10000"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int errorImageText: 48
        }
    }

    // DashboardToolbar.qml
    readonly property QtObject dashboardToolbar: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color titleHover: Qt.rgba(1, 1, 1, 0.08)
            readonly property color divider: "#2a2a2a"
            readonly property color searchBackground: Qt.rgba(1, 1, 1, 0.06)
            readonly property color searchBorder: Qt.rgba(1, 1, 1, 0.10)
            readonly property color searchIcon: "#888"
            readonly property color searchText: "#999999"

            readonly property color pillBackground: Qt.rgba(1, 1, 1, 0.06)
            readonly property color pillBorder: Qt.rgba(1, 1, 1, 0.10)
            readonly property color pillOpen: Qt.rgba(1, 1, 1, 0.12)
            readonly property color pillPressed: Qt.rgba(1, 1, 1, 0.14)
            readonly property color pillHover: Qt.rgba(1, 1, 1, 0.10)
            readonly property color pillBorderOpen: Qt.rgba(1, 1, 1, 0.20)
            readonly property color pillBorderPressed: Qt.rgba(1, 1, 1, 0.22)
            readonly property color pillBorderHover: Qt.rgba(1, 1, 1, 0.18)
            readonly property color pillLabel: "#aaaacc"
            readonly property color pillValueActive: "white"

            readonly property color segmentActive: Qt.rgba(1, 1, 1, 0.15)
            readonly property color segmentPressed: Qt.rgba(1, 1, 1, 0.12)
            readonly property color segmentHover: Qt.rgba(1, 1, 1, 0.08)
            readonly property color segmentLabel: "#888899"
            readonly property color segmentLabelActive: "white"
            readonly property color segmentBackground: "transparent"

            readonly property color chipsLabel: "#888899"
            readonly property color chipsActive: Qt.rgba(1, 1, 1, 0.15)
            readonly property color chipsPressed: Qt.rgba(1, 1, 1, 0.14)
            readonly property color chipsHover: Qt.rgba(1, 1, 1, 0.10)
            readonly property color chipsBorderActive: Qt.rgba(1, 1, 1, 0.25)
            readonly property color chipsBorderPressed: Qt.rgba(1, 1, 1, 0.22)
            readonly property color chipsBorderHover: Qt.rgba(1, 1, 1, 0.18)
            readonly property color chipsText: "#aaaacc"
            readonly property color chipsTextActive: "white"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int searchIcon: 16
            readonly property int searchInput: 13
            readonly property int pillLabel: 13
            readonly property int pillValue: 12
            readonly property int pillOrderValue: 18
            readonly property int segmentLabel: 12
            readonly property int chipsLabel: 11
            readonly property int chipsText: 11
        }
    }

    // CardRow.qml
    readonly property QtObject cardRow: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color rowBackgroundHover: Qt.rgba(1, 1, 1, 0.02)
            readonly property color rowBackgroundPressed: Qt.rgba(1, 1, 1, 0.04)
            readonly property color rowBackground: "transparent"
            readonly property color rowBorder: Qt.rgba(1, 1, 1, 0.08)

            readonly property color iconBackground: "#2a2a3a"
            readonly property color loadingOverlay: "#000000"
            readonly property color completedRing: "#55cc88"
            readonly property color fallbackBackground: "#3a3a5a"
            readonly property color fallbackText: "#aaaacc"

            readonly property color titleText: "#ffffff"
            readonly property color fractionText: "#888899"
            readonly property color completedText: "#55cc88"
            readonly property color star: "#55cc88"
            readonly property color lastPlayed: "#666677"
            readonly property color recentUnlock: "#666677"

            readonly property color achievementsProgressTrack: "#2a2a3a"
            readonly property color achievementsProgressFill: "#55cc88"

            readonly property color installationStatusBackgroundInstalled: Qt.rgba(0.2, 0.7, 0.4, 0.25)
            readonly property color installationStatusBackgroundDefault: Qt.rgba(0.5, 0.5, 0.5, 0.15)
            readonly property color installationStatusTextInstalled: "#55cc88"
            readonly property color installationStatusTextNotInstalled: "#888899"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int fallbackText: 18
            readonly property int title: 13
            readonly property int fraction: 11
            readonly property int star: 16
            readonly property int lastPlayed: 11
            readonly property int recentUnlock: 11
            readonly property int status: 10
        }
    }

    // CardRowDetailed.qml
    readonly property QtObject cardRowDetailed: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color rowBackgroundHover: Qt.rgba(1, 1, 1, 0.02)
            readonly property color rowBackgroundPressed: Qt.rgba(1, 1, 1, 0.04)
            readonly property color rowBackground: "transparent"
            readonly property color rowBorder: Qt.rgba(1, 1, 1, 0.08)

            readonly property color iconBackground: "#2a2a3a"
            readonly property color loadingOverlay: "#000000"
            readonly property color completedRing: "#55cc88"
            readonly property color fallbackBackground: "#3a3a5a"
            readonly property color fallbackText: "#aaaacc"

            readonly property color titleText: "#ffffff"
            readonly property color fractionText: "#888899"
            readonly property color completedText: "#55cc88"
            readonly property color star: "#55cc88"
            readonly property color lastPlayed: "#666677"
            readonly property color recentUnlock: "#666677"

            readonly property color achievementsProgressTrack: "#2a2a3a"
            readonly property color achievementsProgressFill: "#55cc88"

            readonly property color installationStatusBackgroundInstalled: Qt.rgba(0.2, 0.7, 0.4, 0.25)
            readonly property color installationStatusBackgroundDefault: Qt.rgba(0.5, 0.5, 0.5, 0.15)
            readonly property color installationStatusTextInstalled: "#55cc88"
            readonly property color installationStatusTextNotInstalled: "#888899"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int fallbackText: 18
            readonly property int title: 13
            readonly property int fraction: 11
            readonly property int star: 16
            readonly property int lastPlayed: 11
            readonly property int recentUnlock: 11
            readonly property int status: 10
        }
    }

    // CardSmall.qml
    readonly property QtObject cardSmall: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color cardBackground: "transparent"
            readonly property color cover: "#2a2a3a"
            readonly property color loadingOverlay: "#000000"
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color rootHoverOverlay: "transparent"
            readonly property color rootHoverGradientStart: "transparent"
            readonly property color rootHoverGradientEnd: Qt.rgba(0, 0, 0, 0.9)

            readonly property color hoverTitle: "white"
            readonly property color hoverLastPlayed: "#cccccc"
            readonly property color hoverAchievements: "#cccccc"

            readonly property color achievementsBadgeGradientStart: "transparent"
            readonly property color achievementsBadgeGradientEnd: Qt.rgba(0, 0, 0, 0.85)
            readonly property color achievementsBadgeText: "#aaaaaa"

            readonly property color installationStatusBadgeBackground: "black"
            readonly property color rootDropshadow: "#000000"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int imageError: 10
            readonly property int titleFallback: 12
            readonly property int hoverTitle: 13
            readonly property int hoverMeta: 11
            readonly property int achievementsBadge: 10
        }

        readonly property QtObject opacity: QtObject {
            readonly property real statusBadge: 0.2
            readonly property real statusIcon: 0.85
            readonly property real shadowHover: 0.55
            readonly property real shadowIdle: 0.0
        }
    }

    // Card.qml
    readonly property QtObject card: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color cardBackground: "transparent"
            readonly property color cover: "#2a2a3a"
            readonly property color loadingOverlay: "#000000"
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color rootHoverOverlay: "transparent"
            readonly property color rootHoverGradientStart: "transparent"
            readonly property color rootHoverGradientEnd: Qt.rgba(0, 0, 0, 0.9)

            readonly property color hoverTitle: "white"
            readonly property color hoverLastPlayed: "#cccccc"
            readonly property color hoverAchievements: "#aaaaaa"

            readonly property color achievementsBadgeGradientStart: "transparent"
            readonly property color achievementsBadgeGradientEnd: Qt.rgba(0, 0, 0, 0.85)
            readonly property color achievementsBadgeText: "#cccccc"

            readonly property color installationStatusBadgeBackground: "black"
            readonly property color rootDropshadow: "#000000"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int imageError: 11
            readonly property int titleFallback: 14
            readonly property int hoverTitle: 15
            readonly property int hoverMeta: 13
            readonly property int achievementsBadge: 12
        }

        readonly property QtObject opacity: QtObject {
            readonly property real statusBadge: 0.2
            readonly property real statusIcon: 0.85
            readonly property real shadowHover: 0.55
            readonly property real shadowIdle: 0.0
        }
    }

    // TargetDetails.qml
    readonly property QtObject targetDetails: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color text: "#ffffff"
            readonly property color divider: "#2a2a2a"
            readonly property color coverBackground: "#2a2a3a"
            readonly property color coverFallbackText: "white"
            readonly property color progressBarTrack: "#2a2a3a"
            readonly property color progressBarText: "white"
            readonly property color hiddenHoverOverlay: "white"
            readonly property color errorText: "#f0b8b8"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int progressBar: 13
            readonly property int coverFallback: 15
            readonly property int emptyState: 16
            readonly property int metaIcon: 14
            readonly property int metaLabel: 14
            readonly property int metaValue: 14
            readonly property int sectionTitle: 15
            readonly property int rowName: 14
            readonly property int rowDescription: 13
            readonly property int rowGlobalPercent: 13
            readonly property int rowGlobalLabel: 11
            readonly property int rowUnlockDate: 13
            readonly property int hiddenIcon: 22
        }
    }

    // TargetSettings.qml
    readonly property QtObject targetSettings: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color overlay: Qt.rgba(0, 0, 0, 0.70)
            readonly property color background: Qt.rgba(0.14, 0.14, 0.14, 0.94)
            readonly property color border: Qt.rgba(1, 1, 1, 0.12)
            readonly property color titleText: "#e6e6e6"
            readonly property color bodyText: "#9b9b9b"
            readonly property color buttonBackground: Qt.rgba(1, 1, 1, 0.04)
            readonly property color buttonBackgroundHover: Qt.rgba(1, 1, 1, 0.08)
            readonly property color buttonBackgroundPressed: Qt.rgba(1, 1, 1, 0.12)
            readonly property color buttonBorder: Qt.rgba(1, 1, 1, 0.09)
            readonly property color buttonBorderHover: Qt.rgba(1, 1, 1, 0.18)
            readonly property color buttonText: "#e6e6e6"
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: "#f0b8b8"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 16
            readonly property int body: 12
            readonly property int button: 13
        }
    }

    // TargetAchievementEditPopup.qml
    readonly property QtObject targetAchievementEditPopup: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color overlay: Qt.rgba(0, 0, 0, 0.70)
            readonly property color background: Qt.rgba(0.14, 0.14, 0.14, 0.96)
            readonly property color border: Qt.rgba(1, 1, 1, 0.12)
            readonly property color titleText: "#e6e6e6"
            readonly property color bodyText: "#9b9b9b"
            readonly property color labelText: "#cfcfcf"
            readonly property color inputBackground: Qt.rgba(1, 1, 1, 0.04)
            readonly property color inputBorder: Qt.rgba(1, 1, 1, 0.09)
            readonly property color inputBorderFocus: Qt.rgba(1, 1, 1, 0.20)
            readonly property color buttonBackground: Qt.rgba(1, 1, 1, 0.04)
            readonly property color buttonBackgroundHover: Qt.rgba(1, 1, 1, 0.08)
            readonly property color buttonBackgroundPressed: Qt.rgba(1, 1, 1, 0.12)
            readonly property color buttonBorder: Qt.rgba(1, 1, 1, 0.09)
            readonly property color buttonBorderHover: Qt.rgba(1, 1, 1, 0.18)
            readonly property color buttonText: "#e6e6e6"
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: "#f0b8b8"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 16
            readonly property int body: 12
            readonly property int label: 11
            readonly property int input: 13
            readonly property int button: 13
        }
    }

    // ConfirmationPopup.qml
    readonly property QtObject confirmationPopup: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color overlay: Qt.rgba(0, 0, 0, 0.70)
            readonly property color background: Qt.rgba(0.14, 0.14, 0.14, 0.94)
            readonly property color border: Qt.rgba(1, 1, 1, 0.12)
            readonly property color titleText: "#e6e6e6"
            readonly property color bodyText: "#9b9b9b"
            readonly property color buttonBackground: Qt.rgba(1, 1, 1, 0.04)
            readonly property color buttonBackgroundHover: Qt.rgba(1, 1, 1, 0.08)
            readonly property color buttonBackgroundPressed: Qt.rgba(1, 1, 1, 0.12)
            readonly property color buttonBorder: Qt.rgba(1, 1, 1, 0.09)
            readonly property color buttonBorderHover: Qt.rgba(1, 1, 1, 0.18)
            readonly property color buttonText: "#e6e6e6"
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: "#f0b8b8"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 16
            readonly property int body: 12
            readonly property int button: 13
        }
    }

    // NewTarget.qml
    readonly property QtObject newTarget: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color cardBackground: Qt.rgba(1, 1, 1, 0.02)
            readonly property color cardBackgroundHover: Qt.rgba(1, 1, 1, 0.04)
            readonly property color cardBackgroundPressed: Qt.rgba(1, 1, 1, 0.06)
            readonly property color cardBorder: Qt.rgba(1, 1, 1, 0.07)
            readonly property color cardBorderHover: Qt.rgba(1, 1, 1, 0.14)
            readonly property color labelText: "#ffffff"
            readonly property color descriptionText: "#b0b0b0"
            readonly property color disabledText: "#777777"
            readonly property color badgeBackground: Qt.rgba(1, 1, 1, 0.07)
            readonly property color badgeBorder: Qt.rgba(1, 1, 1, 0.10)
            readonly property color badgeText: "#b8b8b8"
            readonly property color icon: "#ffffff"
            readonly property color arrow: "#999999"
            readonly property color arrowHover: "#ffffff"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int label: 16
            readonly property int description: 13
            readonly property int icon: 26
            readonly property int arrow: 22
            readonly property int badge: 11
        }
    }

    // Emulator.qml
    readonly property QtObject emulatorTarget: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color labelText: "#ffffff"
            readonly property color descriptionText: "#999999"
            readonly property color errorText: "#d35f5f"
            readonly property color descriptionMutedText: Qt.rgba(1, 1, 1, 0.3)
            readonly property color divider: "#2a2a2a"
            readonly property color infoBlockBackground: Qt.rgba(1, 1, 1, 0.02)
            readonly property color infoBlockBackgroundHover: Qt.rgba(1, 1, 1, 0.04)
            readonly property color infoBlockBorder: Qt.rgba(1, 1, 1, 0.07)
            readonly property color infoBlockBorderHover: Qt.rgba(1, 1, 1, 0.12)
            readonly property color infoIconBorder: Qt.rgba(1, 1, 1, 0.35)
            readonly property color infoIconText: Qt.rgba(1, 1, 1, 0.35)
            readonly property color infoHeaderInactiveText: Qt.rgba(1, 1, 1, 0.45)
            readonly property color resultBackground: Qt.rgba(1, 1, 1, 0.02)
            readonly property color resultBackgroundHover: Qt.rgba(1, 1, 1, 0.04)
            readonly property color resultBackgroundPressed: Qt.rgba(1, 1, 1, 0.06)
            readonly property color resultBorder: Qt.rgba(1, 1, 1, 0.07)
            readonly property color resultBorderSelected: Qt.rgba(1, 1, 1, 0.14)
            readonly property color prefixWarningText: "#e8a838"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int label: 13
            readonly property int description: 11
            readonly property int descriptionSubtle: 10
            readonly property int infoIcon: 9
            readonly property int input: 15
            readonly property int confirmButton: 13
        }
    }
}
