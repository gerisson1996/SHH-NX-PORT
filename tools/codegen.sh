#!/bin/bash
# Runs the code generator on the app tree and the step the build depends on.
#
#   REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app
#
# Set PYTHON if the Python launcher is not "python" (on Windows it is usually "py").
set -e
RAIZ=$(cd "$(dirname "$0")/.." && pwd)
APP=$RAIZ/${1:?usage: tools/codegen.sh <app folder>}
REXGLUE=${REXGLUE:?set REXGLUE to the rexglue executable built from sdk/}
PY=${PYTHON:-python}
cd "$APP"
echo "== codegen ($APP)"
"$REXGLUE" codegen shh_manifest.toml > codegen.log 2>&1 || { echo "codegen failed, see $APP/codegen.log"; exit 1; }
cd "$RAIZ"
echo "== direct calls"
"$PY" tools/llamadas_directas.py --gen "$APP/generated/default"
echo "done"
