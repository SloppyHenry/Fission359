#!/usr/bin/env bash
# SPDX-FileCopyrightText: Fission3D contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Notarises a signed disk image with Apple and staples the ticket.
#
#   packaging/macos/notarize.sh <file.dmg>
#
# Needs a paid Apple Developer account and a Developer ID signature (package.sh with
# FISSION_CODESIGN_IDENTITY). Credentials come from the environment (CI secrets):
#   FISSION_NOTARY_APPLE_ID, FISSION_NOTARY_TEAM_ID, FISSION_NOTARY_PASSWORD (app-specific password)
# Without them this step is skipped with exit code 0, so CI stays green on the free account.

set -euo pipefail

DMG="${1:?disk image}"

if [ -z "${FISSION_NOTARY_APPLE_ID:-}" ] || [ -z "${FISSION_NOTARY_TEAM_ID:-}" ] \
   || [ -z "${FISSION_NOTARY_PASSWORD:-}" ]; then
    echo "Notarisierung übersprungen / notarisation skipped: keine Zugangsdaten gesetzt / no credentials."
    echo "(Das ist mit dem kostenlosen Entwicklerkonto erwartet. Siehe docs/paketierung.md.)"
    exit 0
fi

echo "Sende an Apple / submitting to Apple (kann einige Minuten dauern / may take minutes) ..."
xcrun notarytool submit "$DMG" \
    --apple-id "$FISSION_NOTARY_APPLE_ID" \
    --team-id "$FISSION_NOTARY_TEAM_ID" \
    --password "$FISSION_NOTARY_PASSWORD" \
    --wait
xcrun stapler staple "$DMG"
xcrun stapler validate "$DMG"
echo "Notarisiert / notarised: $DMG"
