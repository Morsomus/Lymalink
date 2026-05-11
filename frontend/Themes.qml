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
            readonly property int link: 14
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
            readonly property color labelText: "#d8d8d8"
            readonly property color sectionTitle: "#666677"
            readonly property color sectionInfo: "#666677"
            readonly property color applyFlash: "#55cc88"
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
            readonly property color navTextSelected: "#ffffff"
            readonly property color navBackground: "transparent"
            readonly property color navBackgroundHover: "#252525"
            readonly property color navBackgroundSelected: "#001b74"
            readonly property color navBackgroundPressed: "#151515"
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
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color edgeFrame: "transparent"
            readonly property color edgeFrameGray: Qt.rgba(0.50, 0.50, 0.50, 0.70)
            readonly property color edgeFrameGlowA: Qt.rgba(0.70, 0.62, 0.15, 0.85)
            readonly property color edgeFrameGlowB: Qt.rgba(1.00, 0.95, 0.55, 1.00)
            readonly property color edgeFrameDone: Qt.rgba(1.00, 0.80, 0.40, 0.90)
            readonly property color edgeFrameBack: Qt.rgba(0, 0, 0, 0.85)
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
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color edgeFrame: "transparent"
            readonly property color edgeFrameGray: Qt.rgba(0.50, 0.50, 0.50, 0.70)
            readonly property color edgeFrameGlowA: Qt.rgba(0.70, 0.62, 0.15, 0.85)
            readonly property color edgeFrameGlowB: Qt.rgba(1.00, 0.95, 0.55, 1.00)
            readonly property color edgeFrameDone: Qt.rgba(1.00, 0.80, 0.40, 0.90)
            readonly property color edgeFrameBack: Qt.rgba(0, 0, 0, 0.85)
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
}
