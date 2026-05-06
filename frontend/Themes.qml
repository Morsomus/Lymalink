pragma Singleton
import QtQuick

QtObject {
    id: instance

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
            readonly property int emptyTitle: 18
            readonly property int emptyBody: 13
        }
    }

    // Settings.qml
    readonly property QtObject settings: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color divider: "#2a2a2a"
            readonly property color labelText: "#d8d8d8"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int label: 14
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

            readonly property color progressTrack: "#2a2a3a"
            readonly property color progressFill: "#55cc88"

            readonly property color statusBackgroundInstalled: Qt.rgba(0.2, 0.7, 0.4, 0.25)
            readonly property color statusBackgroundDefault: Qt.rgba(0.5, 0.5, 0.5, 0.15)
            readonly property color statusTextInstalled: "#55cc88"
            readonly property color statusTextDefault: "#888899"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int fallbackText: 18
            readonly property int title: 13
            readonly property int fraction: 11
            readonly property int star: 16
            readonly property int lastPlayed: 11
            readonly property int status: 10
        }
    }

    // CardSmall.qml
    readonly property QtObject cardSmall: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color cardBackground: "transparent"
            readonly property color coverPlaceholder: "#2a2a3a"
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color edgeFrame: "transparent"
            readonly property color hoverOverlay: "transparent"
            readonly property color hoverGradientStart: "transparent"
            readonly property color hoverGradientEnd: Qt.rgba(0, 0, 0, 0.9)

            readonly property color hoverTitle: "white"
            readonly property color hoverLastPlayed: "#cccccc"
            readonly property color hoverAchievements: "#aaaaaa"

            readonly property color badgeGradientStart: "transparent"
            readonly property color badgeGradientEnd: Qt.rgba(0, 0, 0, 0.85)
            readonly property color badgeText: "white"

            readonly property color statusBadgeBackground: "black"
            readonly property color shadow: "#000000"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int imageError: 10
            readonly property int titleFallback: 13
            readonly property int hoverTitle: 12
            readonly property int hoverMeta: 10
            readonly property int badge: 10
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
            readonly property color coverPlaceholder: "#2a2a3a"
            readonly property color imageErrorBlock: "#555"
            readonly property color imageErrorText: "#aaa"
            readonly property color titleFallback: "white"
            readonly property color maskFill: "white"

            readonly property color edgeFrame: "transparent"
            readonly property color hoverOverlay: "transparent"
            readonly property color hoverGradientStart: "transparent"
            readonly property color hoverGradientEnd: Qt.rgba(0, 0, 0, 0.9)

            readonly property color hoverTitle: "white"
            readonly property color hoverLastPlayed: "#cccccc"
            readonly property color hoverAchievements: "#aaaaaa"

            readonly property color badgeGradientStart: "transparent"
            readonly property color badgeGradientEnd: Qt.rgba(0, 0, 0, 0.85)
            readonly property color badgeText: "white"

            readonly property color statusBadgeBackground: "black"
            readonly property color shadow: "#000000"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int imageError: 11
            readonly property int titleFallback: 14
            readonly property int hoverTitle: 13
            readonly property int hoverMeta: 11
            readonly property int badge: 12
        }

        readonly property QtObject opacity: QtObject {
            readonly property real statusBadge: 0.2
            readonly property real statusIcon: 0.85
            readonly property real shadowHover: 0.55
            readonly property real shadowIdle: 0.0
        }
    }

}
