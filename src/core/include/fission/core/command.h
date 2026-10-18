// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <string>

namespace fission::core {

class Document;

/// What a step of the history says about itself. Never finished text (rule 4): the interface
/// composes the sentence in German or English from the key and the values.
///
/// Keys: addFeature, removeFeature, setValue, setLabel, suppress, unsuppress, nameBody,
/// colorBody, showBody, hideBody, addMeasure, removeMeasure, setMeasure, renameMeasure,
/// group (several changes made as one), moveBody (a body moved; a transaction of the
/// interface, AP-1.6).
/// Values (all optional, all text): type ("box"), number (feature number), field (value name),
/// value (new expression), name.
struct CommandDescription {
    std::string key;
    std::map<std::string, std::string> values;

    bool operator==(const CommandDescription&) const = default;
};

enum class ApplyResult {
    applied,   ///< The document changed.
    unchanged, ///< Valid, but the document already was in the requested state.
    refused,   ///< Not possible (unknown id, ...); the document is untouched.
};

/// One change of the document, on the level of the document (a value, a step, a name), never
/// of geometry: the geometry is calculated again or comes from the cache (architecture §3.5).
///
/// A command may be applied again after `revert` (that is redo) and must then lead to exactly
/// the same document, same ids included.
class Command {
public:
    virtual ~Command() = default;

    virtual ApplyResult apply(Document& document) = 0;
    /// Undoes `apply`. Only called after `apply` returned `applied`, and only when the document
    /// is in the state `apply` left it in.
    virtual void revert(Document& document) = 0;
    virtual CommandDescription describe() const = 0;

    /// Within a transaction: `next` was applied right after this command. Returns true when
    /// this command now stands for both (for a value dragged around: first old value, last new
    /// value), so the transaction does not keep 200 intermediate steps.
    virtual bool absorb(const Command& next) {
        (void)next;
        return false;
    }
    /// True when applying and reverting would change nothing (after `absorb`).
    virtual bool changesNothing() const { return false; }
};

} // namespace fission::core
