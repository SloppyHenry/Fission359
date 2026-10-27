// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace fission::ui {

/// How well a search text fits one command: 0 = not at all. Pure function, no Qt objects, so it
/// is tested on its own. `words` are the name and the synonyms (German and English together; a
/// synonym may be several words, "runde ecken"). The text is judged word by word: whole word >
/// beginning of a word > contained in a word > a typo (one letter off, for words of 4 or more
/// letters). Capitals, umlauts (ä = a) and ß (= ss) do not matter. Every word of the text must
/// fit something, otherwise 0. `name` counts more than a synonym.
int commandScore(const QString& query, const QString& name, const QStringList& words);

/// The text as the search compares it: lower case, ä ö ü → a o u, ß → ss, letters and digits
/// only, words separated by one space.
QString searchForm(const QString& text);

/// The command search (singleton "CommandSearch" in QML).
class CommandSearch : public QObject {
    Q_OBJECT

public:
    explicit CommandSearch(QObject* parent = nullptr) : QObject(parent) {}

    /// Ranks `commands` ([{ id, name, words: [string] }]) for the text: the ids of those that fit,
    /// best first (equal scores keep the order of the list). An empty text gives all of them.
    Q_INVOKABLE QStringList search(const QString& query, const QVariantList& commands) const;
};

} // namespace fission::ui
