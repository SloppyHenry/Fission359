// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/command_search.h"

#include <QVariantMap>

#include <algorithm>
#include <utility>
#include <vector>

namespace fission::ui {

QString searchForm(const QString& text) {
    QString result;
    result.reserve(text.size());
    bool space = true;
    for (const QChar original : text.toLower()) {
        QString piece;
        switch (original.unicode()) {
        case 0x00E4: // ä
            piece = QStringLiteral("a");
            break;
        case 0x00F6: // ö
            piece = QStringLiteral("o");
            break;
        case 0x00FC: // ü
            piece = QStringLiteral("u");
            break;
        case 0x00DF: // ß
            piece = QStringLiteral("ss");
            break;
        default:
            if (original.isLetterOrNumber()) {
                piece = QString(original);
            }
            break;
        }
        if (piece.isEmpty()) {
            if (!space) {
                result += QLatin1Char(' ');
                space = true;
            }
        } else {
            result += piece;
            space = false;
        }
    }
    return result.trimmed();
}

namespace {

// Edit distance, but only whether it is at most one (substitution, insertion or deletion).
bool withinOneEdit(const QString& a, const QString& b) {
    const int difference = a.size() - b.size();
    if (difference > 1 || difference < -1) {
        return false;
    }
    int i = 0;
    int j = 0;
    int edits = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            ++i;
            ++j;
            continue;
        }
        if (++edits > 1) {
            return false;
        }
        if (a.size() > b.size()) {
            ++i;
        } else if (a.size() < b.size()) {
            ++j;
        } else {
            ++i;
            ++j;
        }
    }
    return edits + (a.size() - i) + (b.size() - j) <= 1;
}

// How well one word of the text fits one word of a command.
int wordScore(const QString& typed, const QString& word) {
    if (typed == word) {
        return 100;
    }
    if (typed.size() >= 2 && word.startsWith(typed)) {
        return 80;
    }
    if (typed.size() >= 3 && word.contains(typed)) {
        return 55;
    }
    if (typed.size() >= 4 && word.size() >= 4 && withinOneEdit(typed, word)) {
        return 40;
    }
    // A typo in the first letters of a long word: compare with the beginning of the word.
    if (typed.size() >= 5 && word.size() > typed.size() &&
        withinOneEdit(typed, word.left(typed.size()))) {
        return 35;
    }
    return 0;
}

} // namespace

int commandScore(const QString& query, const QString& name, const QStringList& words) {
    const QStringList typed = searchForm(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (typed.isEmpty()) {
        return 1; // nothing typed: everything fits a little, the list keeps its order
    }
    const QString nameForm = searchForm(name);
    std::vector<std::pair<QString, int>> candidates; // each word with its weight (percent)
    for (const QString& word : nameForm.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        candidates.emplace_back(word, 150);
    }
    int phrase = 0;
    const QString whole = searchForm(query);
    if (nameForm == whole) {
        phrase = 200; // the name itself
    }
    for (const QString& synonym : words) {
        const QString form = searchForm(synonym);
        if (form == whole) {
            phrase = std::max(phrase, 120); // a synonym that is exactly what was typed
        }
        for (const QString& word : form.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            candidates.emplace_back(word, 100);
        }
    }
    int total = 0;
    for (const QString& word : typed) {
        int best = 0;
        for (const auto& [candidate, weight] : candidates) {
            best = std::max(best, wordScore(word, candidate) * weight / 100);
        }
        if (best == 0) {
            return 0; // a word of the text that fits nothing: the command is not meant
        }
        total += best;
    }
    return total + phrase;
}

QStringList CommandSearch::search(const QString& query, const QVariantList& commands) const {
    struct Hit {
        QString id;
        int score;
        int order;
    };
    std::vector<Hit> hits;
    int order = 0;
    for (const QVariant& entry : commands) {
        const QVariantMap command = entry.toMap();
        const int score = commandScore(query, command.value(QStringLiteral("name")).toString(),
                                       command.value(QStringLiteral("words")).toStringList());
        if (score > 0) {
            hits.push_back({command.value(QStringLiteral("id")).toString(), score, order});
        }
        ++order;
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        return a.score != b.score ? a.score > b.score : a.order < b.order;
    });
    QStringList ids;
    for (const Hit& hit : hits) {
        ids.push_back(hit.id);
    }
    return ids;
}

} // namespace fission::ui
