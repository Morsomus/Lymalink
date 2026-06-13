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
    id: id_themes

    property string activeMode: "dark"
    readonly property bool isLight: activeMode === "light"

    function themeColor(darkColor, lightColor) {
        return isLight ? lightColor : darkColor
    }

    // Main.qml shell
    readonly property QtObject appShell: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color windowBackground: id_themes.themeColor("#181818", "#e4e9ee")
            readonly property color contentBackground: id_themes.themeColor("#1f1f1f", "#e4e9ee")
            readonly property color contentDivider: id_themes.themeColor("#2a2a2a", "#d8dee6")
        }
    }

    // General
    readonly property QtObject general: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: id_themes.themeColor("#202020", "#ffffff")
            readonly property color backgroundHover: id_themes.themeColor("#252525", "#f3f5f7")
            readonly property color backgroundPressed: id_themes.themeColor("#2a2a2a", "#e9edf2")
            readonly property color border: id_themes.themeColor("#2d2d2d", "#d8dee6")
            readonly property color statusActive: "#47d17c"
            readonly property color statusInactive: "#777777"
            readonly property color titleText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color bodyText: id_themes.themeColor("#9b9b9b", "#4b5563")
            readonly property color tooltipBackground: id_themes.themeColor("#222222", "#ffffff")
            readonly property color linkText: id_themes.themeColor("#dcdcdc", "#1f2933")
            readonly property color linkBackground: "transparent"
            readonly property color linkBackgroundHover: id_themes.themeColor("#252525", "#f3f5f7")
            readonly property color linkBackgroundPressed: id_themes.themeColor("#151515", "#e9edf2")
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

    // CustomComboBox.qml
    readonly property QtObject customComboBox: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: id_themes.themeColor("#1f2326", "#ffffff")
            readonly property color backgroundHover: id_themes.themeColor("#252b2f", "#f3f6f8")
            readonly property color backgroundPressed: id_themes.themeColor("#1a1e21", "#e8edf2")
            readonly property color border: id_themes.themeColor("#3b454e", "#9aa9b7")
            readonly property color borderActive: id_themes.themeColor("#2e99d6", "#0f78b7")
            readonly property color popupBackground: id_themes.themeColor("#1f2326", "#ffffff")
            readonly property color rowHover: id_themes.themeColor("#26333b", "#e7f1f8")
            readonly property color rowSelected: id_themes.themeColor("#113d55", "#d6ecf8")
            readonly property color text: id_themes.themeColor("#d8d8d8", "#1f2933")
            readonly property color textDisabled: id_themes.themeColor("#666677", "#667085")
            readonly property color indicator: id_themes.themeColor("#d8d8d8", "#1f2933")
            readonly property color indicatorDisabled: id_themes.themeColor("#666677", "#667085")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int text: 13
            readonly property int rowText: 13
        }
    }

    // CustomSwitch.qml
    readonly property QtObject customSwitch: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color trackOn: id_themes.themeColor("#3281bf", "#2f7ebc")
            readonly property color trackOff: id_themes.themeColor("#3d444c", "#d2d8de")
            readonly property color trackDisabled: id_themes.themeColor("#2f343a", "#c2c8ce")
            readonly property color trackBorder: id_themes.themeColor("#57616c", "#a8b2bd")
            readonly property color handleOn: id_themes.themeColor("#25292d", "#ffffff")
            readonly property color handleOff: id_themes.themeColor("#25292d", "#f7f9fb")
            readonly property color handleDisabled: id_themes.themeColor("#505860", "#e2e6ea")
            readonly property color handleBorder: id_themes.themeColor("#5b6570", "#8c98a4")
            readonly property color handleShadow: id_themes.themeColor("#000000", "#65707a")
            readonly property color text: id_themes.themeColor("#d8d8d8", "#1f2933")
            readonly property color textDisabled: id_themes.themeColor("#666677", "#667085")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int text: 13
        }
    }

    // CustomCheckBox.qml
    readonly property QtObject customCheckBox: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: id_themes.themeColor("#25292d", "#ffffff")
            readonly property color backgroundHover: id_themes.themeColor("#2d3338", "#f3f6f8")
            readonly property color backgroundPressed: id_themes.themeColor("#1f2327", "#e8edf2")
            readonly property color backgroundChecked: id_themes.themeColor("#2d3338", "#2f7ebc")
            readonly property color backgroundDisabled: id_themes.themeColor("#202428", "#eef1f4")
            readonly property color border: id_themes.themeColor("#4d5964", "#a8b4bf")
            readonly property color borderHover: id_themes.themeColor("#5e6b77", "#8797a6")
            readonly property color borderChecked: id_themes.themeColor("#4d5964", "#2f7ebc")
            readonly property color borderFocus: id_themes.themeColor("#2e99d6", "#0f78b7")
            readonly property color mark: "#ffffff"
            readonly property color markDisabled: id_themes.themeColor("#9aa3ad", "#ffffff")
            readonly property color text: id_themes.themeColor("#d8d8d8", "#1f2933")
            readonly property color textDisabled: id_themes.themeColor("#666677", "#667085")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int text: 13
        }
    }

    // CustomButton.qml
    readonly property QtObject customButton: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: id_themes.themeColor("#25292d", "#ffffff")
            readonly property color backgroundHover: id_themes.themeColor("#2d3338", "#f3f6f8")
            readonly property color backgroundPressed: id_themes.themeColor("#1f2327", "#e8edf2")
            readonly property color backgroundDisabled: id_themes.themeColor("#202428", "#eef1f4")
            readonly property color border: id_themes.themeColor("#4d5964", "#a8b4bf")
            readonly property color borderHover: id_themes.themeColor("#5e6b77", "#8797a6")
            readonly property color borderFocus: id_themes.themeColor("#2e99d6", "#0f78b7")
            readonly property color text: id_themes.themeColor("#f0f2f4", "#1f2933")
            readonly property color textDisabled: id_themes.themeColor("#777f88", "#8a96a3")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int text: 13
        }
    }

    // CustomTextField.qml
    readonly property QtObject customTextField: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color background: id_themes.themeColor("#101214", "#ffffff")
            readonly property color backgroundHover: id_themes.themeColor("#16191c", "#f3f6f8")
            readonly property color backgroundDisabled: id_themes.themeColor("#151719", "#eef1f4")
            readonly property color border: id_themes.themeColor("#3b4249", "#a8b4bf")
            readonly property color borderHover: id_themes.themeColor("#4d5660", "#8797a6")
            readonly property color borderFocus: id_themes.themeColor("#2e99d6", "#0f78b7")
            readonly property color text: id_themes.themeColor("#e7eaed", "#1f2933")
            readonly property color textDisabled: id_themes.themeColor("#747c84", "#8a96a3")
            readonly property color placeholderText: id_themes.themeColor("#8a929a", "#667085")
            readonly property color selectedText: id_themes.themeColor("#f8fafc", "#ffffff")
            readonly property color selection: id_themes.themeColor("#1f6f9f", "#0f78b7")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int text: 13
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
            readonly property color background: id_themes.themeColor("#242424", "#ffffff")
            readonly property color border: "#d35f5f"
            readonly property color titleText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color bodyText: id_themes.themeColor("#9b9b9b", "#4b5563")
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
            readonly property color gold_incomplete_hi: id_themes.themeColor(Qt.rgba(1.00, 0.80, 0.40, 0.85), Qt.rgba(0.62, 0.43, 0.02, 0.95))
            readonly property color gold_doneStatic: id_themes.themeColor(Qt.rgba(1.00, 0.80, 0.40, 0.90), Qt.rgba(0.64, 0.45, 0.04, 0.95))
            readonly property color edgeFrameGold_animA: id_themes.themeColor(Qt.rgba(0.70, 0.62, 0.15, 0.85), Qt.rgba(0.52, 0.38, 0.02, 0.95))
            readonly property color edgeFrameGold_animB: id_themes.themeColor(Qt.rgba(1.00, 0.95, 0.55, 1.00), Qt.rgba(0.66, 0.47, 0.04, 1.00))

            // 1: Electric Blue
            readonly property color blue_incomplete_hi: id_themes.themeColor(Qt.rgba(0.24, 0.42, 1.00, 0.85), Qt.rgba(0.18, 0.36, 0.90, 0.90))
            readonly property color blue_doneStatic: id_themes.themeColor(Qt.rgba(0.30, 0.54, 1.00, 0.90), Qt.rgba(0.22, 0.42, 0.92, 0.94))
            readonly property color edgeFrameBlue_animA: id_themes.themeColor(Qt.rgba(0.10, 0.33, 1.00, 0.85), Qt.rgba(0.12, 0.30, 0.86, 0.92))
            readonly property color edgeFrameBlue_animB: id_themes.themeColor(Qt.rgba(0.53, 0.67, 1.00, 1.00), Qt.rgba(0.30, 0.50, 0.96, 1.00))

            // 2: Purple
            readonly property color purple_incomplete_hi: id_themes.themeColor(Qt.rgba(0.48, 0.18, 1.00, 0.85), Qt.rgba(0.43, 0.18, 0.86, 0.90))
            readonly property color purple_doneStatic: id_themes.themeColor(Qt.rgba(0.62, 0.36, 1.00, 0.90), Qt.rgba(0.50, 0.26, 0.90, 0.94))
            readonly property color edgeFramePurple_animA: id_themes.themeColor(Qt.rgba(0.48, 0.18, 1.00, 0.85), Qt.rgba(0.40, 0.16, 0.82, 0.92))
            readonly property color edgeFramePurple_animB: id_themes.themeColor(Qt.rgba(0.78, 0.60, 1.00, 1.00), Qt.rgba(0.58, 0.36, 0.94, 1.00))

            // 3: Emerald
            readonly property color emerald_incomplete_hi: id_themes.themeColor(Qt.rgba(0.10, 0.48, 0.29, 0.85), Qt.rgba(0.06, 0.46, 0.28, 0.90))
            readonly property color emerald_doneStatic: id_themes.themeColor(Qt.rgba(0.18, 0.74, 0.43, 0.90), Qt.rgba(0.10, 0.56, 0.34, 0.94))
            readonly property color edgeFrameEmerald_animA: id_themes.themeColor(Qt.rgba(0.10, 0.60, 0.33, 0.85), Qt.rgba(0.04, 0.44, 0.26, 0.92))
            readonly property color edgeFrameEmerald_animB: id_themes.themeColor(Qt.rgba(0.36, 1.00, 0.63, 1.00), Qt.rgba(0.08, 0.58, 0.34, 1.00))

            // 4: Ember
            readonly property color ember_incomplete_hi: id_themes.themeColor(Qt.rgba(0.75, 0.20, 0.10, 0.85), Qt.rgba(0.74, 0.18, 0.08, 0.90))
            readonly property color ember_doneStatic: id_themes.themeColor(Qt.rgba(1.00, 0.38, 0.19, 0.90), Qt.rgba(0.86, 0.28, 0.10, 0.94))
            readonly property color edgeFrameEmber_animA: id_themes.themeColor(Qt.rgba(0.87, 0.25, 0.06, 0.85), Qt.rgba(0.72, 0.16, 0.05, 0.92))
            readonly property color edgeFrameEmber_animB: id_themes.themeColor(Qt.rgba(1.00, 0.67, 0.27, 1.00), Qt.rgba(0.80, 0.30, 0.08, 1.00))

            // 5: Frost
            readonly property color frost_incomplete_hi: id_themes.themeColor(Qt.rgba(1.00, 1.00, 1.00, 0.35), Qt.rgba(0.10, 0.48, 0.66, 0.90))
            readonly property color frost_doneStatic: id_themes.themeColor(Qt.rgba(0.78, 0.90, 1.00, 0.75), Qt.rgba(0.14, 0.56, 0.74, 0.94))
            readonly property color edgeFrameFrost_animA: id_themes.themeColor(Qt.rgba(0.78, 0.90, 1.00, 0.60), Qt.rgba(0.08, 0.42, 0.60, 0.92))
            readonly property color edgeFrameFrost_animB: id_themes.themeColor(Qt.rgba(1.00, 1.00, 1.00, 0.90), Qt.rgba(0.10, 0.52, 0.70, 1.00))
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
            if (!isLight) {
                const greyDark = 0.45
                const pDark = Math.max(0.0, Math.min(t, 1.0))
                const hiDark = progressColor(s)
                return Qt.rgba(
                    greyDark + pDark * (hiDark.r - greyDark),
                    greyDark + pDark * (hiDark.g - greyDark),
                    greyDark + pDark * (hiDark.b - greyDark),
                    0.60 + pDark * 0.25
                )
            }

            const grey = 0.32
            const p = Math.max(0.0, Math.min(t, 1.0))
            const hi = progressColor(s)
            return Qt.rgba(
                grey + p * (hi.r - grey),
                grey + p * (hi.g - grey),
                grey + p * (hi.b - grey),
                0.72 + p * 0.18
            )
        }
    }

    // Dashboard.qml
    readonly property QtObject dashboard: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#d8dee6")
            readonly property color bodyText: id_themes.themeColor("#999999", "#4b5563")
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
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#a8b3c1")
            readonly property color subDivider: id_themes.themeColor("#242424", "#c1cad5")
            readonly property color labelText: id_themes.themeColor("#d8d8d8", "#1f2933")
            readonly property color sectionTitle: id_themes.themeColor("#666677", "#667085")
            readonly property color sectionInfo: id_themes.themeColor("#666677", "#667085")
            readonly property color applyFlash: "#55cc88"
            readonly property color infoBoxBorder: id_themes.themeColor("#34363a", "#d8dee6")
            readonly property color infoBoxBackground: id_themes.themeColor("#202124", "#f5f6f8")
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
            readonly property color panel: id_themes.themeColor("#181818", "#cfd5db")
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#9fa4aa")
            readonly property color versionText: id_themes.themeColor("#666666", "#7a8491")

            readonly property color collapseText: id_themes.themeColor("#d0d0d0", "#1f2933")
            readonly property color collapseBackground: id_themes.themeColor("#202020", "#ffffff")
            readonly property color collapseBackgroundHover: id_themes.themeColor("#303030", "#f3f5f7")
            readonly property color collapseBackgroundPressed: id_themes.themeColor("#151515", "#e9edf2")
            readonly property color collapseBorder: id_themes.themeColor("#343434", "#d8dee6")

            readonly property color navText: id_themes.themeColor("#bdbdbd", "#4b5563")
            readonly property color navTextSelected: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color navBackground: "transparent"
            readonly property color navBackgroundHover: id_themes.themeColor("#252525", "#f3f5f7")
            readonly property color navBackgroundSelected: id_themes.themeColor("#001b74", "#dbe7ff")
            readonly property color navBackgroundPressed: id_themes.themeColor("#151515", "#e9edf2")
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
            readonly property color navText: id_themes.themeColor("#bdbdbd", "#4b5563")
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
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color titleHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), Qt.rgba(31 / 255, 41 / 255, 51 / 255, 0.08))
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#a8b3c1")
            readonly property color searchBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.06), "#ffffff")
            readonly property color searchBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.10), "#9aa7b6")
            readonly property color searchIcon: id_themes.themeColor("#888", "#667085")
            readonly property color searchText: id_themes.themeColor("#999999", "#1f2933")

            readonly property color pillBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.06), "#ffffff")
            readonly property color pillBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.10), "#a8b3c1")
            readonly property color pillOpen: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#dce4ed")
            readonly property color pillPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.14), "#dce4ed")
            readonly property color pillHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.10), "#e7edf3")
            readonly property color pillBorderOpen: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.20), "#667085")
            readonly property color pillBorderPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.22), "#667085")
            readonly property color pillBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.18), "#7f8ea1")
            readonly property color pillLabel: id_themes.themeColor("#aaaacc", "#4b5563")
            readonly property color pillValueActive: id_themes.themeColor("white", "#1f2933")

            readonly property color segmentActive: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.15), "#dce4ed")
            readonly property color segmentPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#dce4ed")
            readonly property color segmentHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#e7edf3")
            readonly property color segmentLabel: id_themes.themeColor("#888899", "#667085")
            readonly property color segmentLabelActive: id_themes.themeColor("white", "#1f2933")
            readonly property color segmentBackground: id_themes.themeColor("transparent", "#ffffff")

            readonly property color chipsLabel: id_themes.themeColor("#888899", "#667085")
            readonly property color chipsActive: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.15), "#dce4ed")
            readonly property color chipsPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.14), "#dce4ed")
            readonly property color chipsHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.10), "#e7edf3")
            readonly property color chipsBorderActive: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.25), "#667085")
            readonly property color chipsBorderPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.22), "#667085")
            readonly property color chipsBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.18), "#7f8ea1")
            readonly property color chipsText: id_themes.themeColor("#aaaacc", "#4b5563")
            readonly property color chipsTextActive: id_themes.themeColor("white", "#1f2933")
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
            readonly property color rowBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#edf2f6")
            readonly property color rowBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#dce4ed")
            readonly property color rowBackground: id_themes.themeColor("transparent", "#e4e9ee")
            readonly property color rowBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#a8b3c1")

            readonly property color iconBackground: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color loadingOverlay: "#000000"
            readonly property color completedRing: "#55cc88"
            readonly property color fallbackBackground: id_themes.themeColor("#3a3a5a", "#dbe7ff")
            readonly property color fallbackText: id_themes.themeColor("#aaaacc", "#4b5563")

            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color fractionText: id_themes.themeColor("#888899", "#667085")
            readonly property color completedText: "#55cc88"
            readonly property color star: "#55cc88"
            readonly property color lastPlayed: id_themes.themeColor("#666677", "#7a8491")
            readonly property color recentUnlock: id_themes.themeColor("#666677", "#7a8491")

            readonly property color achievementsProgressTrack: id_themes.themeColor("#2a2a3a", "#cfd8e3")
            readonly property color achievementsProgressFill: "#55cc88"

            readonly property color installationStatusBackgroundInstalled: Qt.rgba(0.2, 0.7, 0.4, 0.25)
            readonly property color installationStatusBackgroundDefault: id_themes.themeColor(Qt.rgba(0.5, 0.5, 0.5, 0.15), Qt.rgba(102 / 255, 112 / 255, 133 / 255, 0.14))
            readonly property color installationStatusTextInstalled: "#55cc88"
            readonly property color installationStatusTextNotInstalled: id_themes.themeColor("#888899", "#667085")
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
            readonly property color rowBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#edf2f6")
            readonly property color rowBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#dce4ed")
            readonly property color rowBackground: id_themes.themeColor("transparent", "#e4e9ee")
            readonly property color rowBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#a8b3c1")

            readonly property color iconBackground: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color loadingOverlay: "#000000"
            readonly property color completedRing: "#55cc88"
            readonly property color fallbackBackground: id_themes.themeColor("#3a3a5a", "#dbe7ff")
            readonly property color fallbackText: id_themes.themeColor("#aaaacc", "#4b5563")

            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color fractionText: id_themes.themeColor("#888899", "#667085")
            readonly property color completedText: "#55cc88"
            readonly property color star: "#55cc88"
            readonly property color lastPlayed: id_themes.themeColor("#666677", "#7a8491")
            readonly property color recentUnlock: id_themes.themeColor("#666677", "#7a8491")

            readonly property color achievementsProgressTrack: id_themes.themeColor("#2a2a3a", "#cfd8e3")
            readonly property color achievementsProgressFill: "#55cc88"

            readonly property color installationStatusBackgroundInstalled: Qt.rgba(0.2, 0.7, 0.4, 0.25)
            readonly property color installationStatusBackgroundDefault: id_themes.themeColor(Qt.rgba(0.5, 0.5, 0.5, 0.15), Qt.rgba(102 / 255, 112 / 255, 133 / 255, 0.14))
            readonly property color installationStatusTextInstalled: "#55cc88"
            readonly property color installationStatusTextNotInstalled: id_themes.themeColor("#888899", "#667085")
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
            readonly property color cover: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color loadingOverlay: "#000000"
            readonly property color imageErrorBlock: id_themes.themeColor("#555", "#d8dee6")
            readonly property color imageErrorText: id_themes.themeColor("#aaa", "#667085")
            readonly property color titleFallback: id_themes.themeColor("white", "#1f2933")
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
            readonly property color cover: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color loadingOverlay: "#000000"
            readonly property color imageErrorBlock: id_themes.themeColor("#555", "#d8dee6")
            readonly property color imageErrorText: id_themes.themeColor("#aaa", "#667085")
            readonly property color titleFallback: id_themes.themeColor("white", "#1f2933")
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
            readonly property color text: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color divider: id_themes.themeColor("#363636", "#667085")
            readonly property color sectionHeaderText: id_themes.themeColor("#ffffff", "#181a1f")
            readonly property color coverBackground: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color coverFallbackText: id_themes.themeColor("white", "#1f2933")
            readonly property color progressBarTrack: id_themes.themeColor("#2a2a3a", "#e9edf2")
            readonly property color progressBarText: id_themes.themeColor("white", "#1f2933")
            readonly property color hiddenHoverOverlay: id_themes.themeColor("white", "#1f2933")
            readonly property color errorText: id_themes.themeColor("#f0b8b8", "#b42318")
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
            readonly property color background: id_themes.themeColor(Qt.rgba(0.14, 0.14, 0.14, 0.94), Qt.rgba(1, 1, 1, 0.96))
            readonly property color border: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#d8dee6")
            readonly property color titleText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color bodyText: id_themes.themeColor("#9b9b9b", "#4b5563")
            readonly property color buttonBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f5f6f8")
            readonly property color buttonBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#f3f5f7")
            readonly property color buttonBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#e9edf2")
            readonly property color buttonBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.09), "#d8dee6")
            readonly property color buttonBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.18), "#b8c2cf")
            readonly property color buttonText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: id_themes.themeColor("#f0b8b8", "#b42318")
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
            readonly property color background: id_themes.themeColor(Qt.rgba(0.14, 0.14, 0.14, 0.96), Qt.rgba(1, 1, 1, 0.96))
            readonly property color border: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#d8dee6")
            readonly property color titleText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color bodyText: id_themes.themeColor("#9b9b9b", "#4b5563")
            readonly property color labelText: id_themes.themeColor("#cfcfcf", "#1f2933")
            readonly property color inputBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f5f6f8")
            readonly property color inputBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.09), "#d8dee6")
            readonly property color inputBorderFocus: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.20), "#b8c2cf")
            readonly property color buttonBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f5f6f8")
            readonly property color buttonBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#f3f5f7")
            readonly property color buttonBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#e9edf2")
            readonly property color buttonBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.09), "#d8dee6")
            readonly property color buttonBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.18), "#b8c2cf")
            readonly property color buttonText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: id_themes.themeColor("#f0b8b8", "#b42318")
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
            readonly property color background: id_themes.themeColor(Qt.rgba(0.14, 0.14, 0.14, 0.94), Qt.rgba(1, 1, 1, 0.96))
            readonly property color border: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#d8dee6")
            readonly property color titleText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color bodyText: id_themes.themeColor("#9b9b9b", "#4b5563")
            readonly property color buttonBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f5f6f8")
            readonly property color buttonBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.08), "#f3f5f7")
            readonly property color buttonBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#e9edf2")
            readonly property color buttonBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.09), "#d8dee6")
            readonly property color buttonBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.18), "#b8c2cf")
            readonly property color buttonText: id_themes.themeColor("#e6e6e6", "#1f2933")
            readonly property color dangerBackground: Qt.rgba(0.85, 0.22, 0.22, 0.12)
            readonly property color dangerBackgroundHover: Qt.rgba(0.85, 0.22, 0.22, 0.20)
            readonly property color dangerBackgroundPressed: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorder: Qt.rgba(0.85, 0.22, 0.22, 0.28)
            readonly property color dangerBorderHover: Qt.rgba(0.85, 0.22, 0.22, 0.45)
            readonly property color dangerText: id_themes.themeColor("#f0b8b8", "#b42318")
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
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color cardBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#ffffff")
            readonly property color cardBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f3f5f7")
            readonly property color cardBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.06), "#e9edf2")
            readonly property color cardBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#d8dee6")
            readonly property color cardBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.14), "#b8c2cf")
            readonly property color labelText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color descriptionText: id_themes.themeColor("#b0b0b0", "#4b5563")
            readonly property color disabledText: id_themes.themeColor("#777777", "#7a8491")
            readonly property color badgeBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#f5f6f8")
            readonly property color badgeBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.10), "#d8dee6")
            readonly property color badgeText: id_themes.themeColor("#b8b8b8", "#4b5563")
            readonly property color icon: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color arrow: id_themes.themeColor("#999999", "#667085")
            readonly property color arrowHover: id_themes.themeColor("#ffffff", "#1f2933")
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
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color labelText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color descriptionText: id_themes.themeColor("#999999", "#4b5563")
            readonly property color errorText: "#d35f5f"
            readonly property color descriptionMutedText: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.3), Qt.rgba(102 / 255, 112 / 255, 133 / 255, 0.65))
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#d8dee6")
            readonly property color infoBlockBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#ffffff")
            readonly property color infoBlockBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f3f5f7")
            readonly property color infoBlockBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#d8dee6")
            readonly property color infoBlockBorderHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.12), "#b8c2cf")
            readonly property color infoIconBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.35), "#7a8491")
            readonly property color infoIconText: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.35), "#7a8491")
            readonly property color infoHeaderInactiveText: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.45), "#667085")
            readonly property color resultBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#ffffff")
            readonly property color resultBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f3f5f7")
            readonly property color resultBackgroundPressed: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.06), "#e9edf2")
            readonly property color resultBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#d8dee6")
            readonly property color resultBorderSelected: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.14), "#b8c2cf")
            readonly property color prefixWarningText: id_themes.themeColor("#e8a838", "#9a5a00")
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

    // SteamImport.qml
    readonly property QtObject steamImportTarget: QtObject {
        readonly property QtObject colors: QtObject {
            readonly property color titleText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color labelText: id_themes.themeColor("#ffffff", "#1f2933")
            readonly property color descriptionText: id_themes.themeColor("#999999", "#4b5563")
            readonly property color descriptionMutedText: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.3), Qt.rgba(102 / 255, 112 / 255, 133 / 255, 0.65))
            readonly property color warningText: "#e8a838"
            readonly property color errorText: "#d35f5f"
            readonly property color divider: id_themes.themeColor("#2a2a2a", "#d8dee6")
            readonly property color infoBlockBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#ffffff")
            readonly property color infoBlockBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#d8dee6")
            readonly property color resultBackground: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.02), "#ffffff")
            readonly property color resultBackgroundHover: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.04), "#f3f5f7")
            readonly property color resultBorder: id_themes.themeColor(Qt.rgba(1, 1, 1, 0.07), "#d8dee6")
            readonly property color importedBadgeBackground: Qt.rgba(0.34, 0.72, 0.48, 0.12)
            readonly property color importedBadgeBorder: Qt.rgba(0.34, 0.72, 0.48, 0.42)
            readonly property color importedBadgeText: id_themes.themeColor("#8dd8a6", "#2e7d4f")
        }

        readonly property QtObject fontSizes: QtObject {
            readonly property int title: 28
            readonly property int label: 13
            readonly property int description: 11
            readonly property int descriptionSubtle: 10
            readonly property int input: 15
            readonly property int badge: 10
        }
    }
}
