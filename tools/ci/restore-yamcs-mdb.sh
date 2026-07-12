#!/usr/bin/env bash
# Restore the YAMCS Mission Database (fprime.xtce.xml) into the path the
# docker compose stack expects, copying from wherever
# actions/download-artifact placed it. Used by the integration and
# yamcs-build CI jobs.
set -euo pipefail

DEST_DIR="yamcs/yamcs-data/mdb"
DEST="${DEST_DIR}/fprime.xtce.xml"

mkdir -p "${DEST_DIR}"
if [ -f "${DEST}" ]; then
  exit 0
fi

src=$(find . -name fprime.xtce.xml -print -quit)
if [ -z "${src}" ]; then
  echo "ERROR: fprime.xtce.xml not found in downloaded artifacts" >&2
  exit 1
fi
cp "${src}" "${DEST}"
