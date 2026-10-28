// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The three ways to combine chosen bodies (UX 7.1, AP-1.7): the name on the button, the sentence
// of its tooltip card, what the hint line says, and the words the command search finds it by
// (German and English at once). Rule 10 of the master prompt: every tool brings these along.
QtObject {
    // `name`: the full name (tooltip card, menu); `label`: the short word under the icon of its button.
    // `needs`: how many bodies must be chosen (1 for splitting; the others work on two or more).
    readonly property var ways: [
        { id: "merge", name: qsTr("Merge"), label: qsTr("Merge"), icon: "combine",
          tip: qsTr("Makes the chosen bodies one body. Choose two or more first."),
          hint: qsTr("Choose two or more bodies, then Merge: they become one body that takes the name and colour of the first."),
          synonyms: { de: ["vereinen", "verbinden", "gruppieren", "zusammenfügen", "zusammenfassen", "verschmelzen"],
                      en: ["merge", "union", "join", "group", "combine", "unite", "add"] } },
        { id: "subtract", name: qsTr("Subtract"), label: qsTr("Subtract"), icon: "hole",
          tip: qsTr("Cuts the other bodies out of the first one you chose. The cutters stay as ghosts you can still move."),
          hint: qsTr("Choose the body to cut first, then the cutters, then Subtract. A cutter stays as a ghost; change it and the cut follows."),
          synonyms: { de: ["ausschneiden", "abziehen", "loch", "bohren", "subtrahieren", "aussparen"],
                      en: ["subtract", "cut", "hole", "drill", "difference", "cut out", "remove"] } },
        { id: "overlap", name: qsTr("Keep overlap"), label: qsTr("Overlap"), icon: "overlap",
          tip: qsTr("Keeps only the part that all chosen bodies have in common."),
          hint: qsTr("Choose two or more bodies that overlap, then Keep overlap: only the shared part is left."),
          synonyms: { de: ["überlappung", "schnittmenge", "schnitt", "gemeinsam", "durchschnitt"],
                      en: ["overlap", "intersect", "intersection", "common", "shared"] } },
        { id: "split", name: qsTr("Split"), label: qsTr("Split"), icon: "scissors", needs: 1,
          tip: qsTr("Cuts one body in two with a plane. Pull the arrow to move the plane; each part can be moved on its own."),
          hint: qsTr("Choose one body, then Split. Pull the arrow or type a position; choose X, Y or Z for the direction of the cut."),
          synonyms: { de: ["teilen", "trennen", "durchschneiden", "halbieren", "schneiden", "zerteilen"],
                      en: ["split", "divide", "slice", "halve", "separate", "cut in two"] } }
    ]

    function find(id) {
        for (let i = 0; i < ways.length; ++i) {
            if (ways[i].id === id) {
                return ways[i]
            }
        }
        return null
    }
}
