/////////////////////////////////////////////////////////
// File: CardList.qml
// Date: 2026-05-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: List view for Dashboard. Displays tracked
//              content as horizontal rows via CardRow.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls

Item {
    id: id_root

    // TERMPORARY: Dummy model
    ListModel {
        id: id_dummyModel

        ListElement { title: "Hollow Warden";           coverSource: ""; logoSource: ""; achievementCount: 45; achievementTotal: 63; status: "Installed"; lastPlayed: "2 days ago"; recentUnlock: "1 hour ago" }
        ListElement { title: "Frostpeak";               coverSource: ""; logoSource: ""; achievementCount: 12; achievementTotal: 24; status: "Installed"; lastPlayed: "1 week ago"; recentUnlock: "" }
        ListElement { title: "Acheron";                 coverSource: ""; logoSource: ""; achievementCount: 0; achievementTotal: 49; status: "Not Installed"; lastPlayed: ""; recentUnlock: "" }
        ListElement { title: "Dissonant Reverie";       coverSource: ""; logoSource: ""; achievementCount: 8; achievementTotal: 27; status: "Installed"; lastPlayed: "Yesterday"; recentUnlock: "" }
        ListElement { title: "Twilight Hollow";         coverSource: "qrc:/qt/qml/Lymalink/res/img/library_600x900_2x.jpg"; logoSource: ""; achievementCount: 30; achievementTotal: 40; status: "Installed"; lastPlayed: "3 days ago"; recentUnlock: "" }
        ListElement { title: "The Lost Meridian";       coverSource: ""; logoSource: ""; achievementCount: 0; achievementTotal: 16; status: "Not Installed"; lastPlayed: ""; recentUnlock: "2 days ago" }
        ListElement { title: "Aris and the Shroudwood"; coverSource: ""; logoSource: ""; achievementCount: 20; achievementTotal: 35; status: "Installed"; lastPlayed: "5 days ago"; recentUnlock: "2 months ago" }
        ListElement { title: "The Mischievous Fowl";    coverSource: ""; logoSource: ""; achievementCount: 5; achievementTotal: 12; status: "Installed"; lastPlayed: "2 weeks ago"; recentUnlock: "" }
        ListElement { title: "Aethelwald III";          coverSource: ""; logoSource: ""; achievementCount: 102; achievementTotal: 102; status: "Installed"; lastPlayed: "1 hour ago"; recentUnlock: "4 weeks ago" }
    }

    // List view
    ListView {
        id: id_listView
        
        anchors.fill: parent
        spacing: 6

        model: id_dummyModel

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: CardRow {
            width:            id_listView.width
            title:            model.title
            coverSource:      model.coverSource
            logoSource:       model.logoSource
            achievementCount: model.achievementCount
            achievementTotal: model.achievementTotal
            status:           model.status
            lastPlayed:       model.lastPlayed
            recentUnlock:     model.recentUnlock
            delegateIndex:    model.index
        }
    }
}
