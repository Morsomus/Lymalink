pragma Singleton
import QtQuick

QtObject {
    id: instance

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

    readonly property QtObject dashboard: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: "#ffffff"
            readonly property color divider: "#2a2a2a"
            readonly property color bodyText: "#999999"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int body: 15
        }
    }

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

    readonly property QtObject sidebar: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color panel: "#181818"
            readonly property color divider: "#2a2a2a"
            readonly property color versionText: "#666666"

            readonly property color collapseText: "#d0d0d0"
            readonly property color collapseBackground: "#202020"
            readonly property color collapseBackgroundHover: "#303030"
            readonly property color collapseBorder: "#343434"

            readonly property color navText: "#bdbdbd"
            readonly property color navTextSelected: "#ffffff"
            readonly property color navBackground: "transparent"
            readonly property color navBackgroundHover: "#252525"
            readonly property color navBackgroundSelected: "#2f80ed"
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int version: 11
            readonly property int collapseButton: 12
            readonly property int navIcon: 16
            readonly property int navLabel: 16
        }
    }
}
