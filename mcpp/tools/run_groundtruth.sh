#!/usr/bin/env bash
# run_groundtruth.sh — Linux counterpart of run_groundtruth.ps1.
# Compiles and runs a Java worldgen parity ground-truth generator against the
# REAL Minecraft 26.1.2 classes, writing a TSV that a C++ *_parity test compares
# against.
#
# Requires the git-ignored real-Java runtime under 26.1.2/ (fetch per AGENTS.md):
#   * 26.1.2/client.jar           (real bytecode, SHA1-verified)
#   * 26.1.2/libs/*.jar           (version-manifest libraries)
#   * 26.1.2/jdk25/bin/java,javac (JDK 25 — client.jar is class v69)
#
# Usage (from repo root):
#   mcpp/tools/run_groundtruth.sh FeatureSorterParity     mcpp/build/feature_sorter.tsv
#   mcpp/tools/run_groundtruth.sh StructurePlacementParity mcpp/build/structure_placement.tsv
set -euo pipefail

TOOL="${1:?usage: run_groundtruth.sh <Tool> <out.tsv> [args...]}"
OUT="${2:?usage: run_groundtruth.sh <Tool> <out.tsv> [args...]}"
shift 2

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
JAR="$REPO/26.1.2/client.jar"
LIBS="$REPO/26.1.2/libs/*"
SRC="$REPO/mcpp/tools/$TOOL.java"
CLASSES="$REPO/26.1.2/parity_classes"
JAVAC="$REPO/26.1.2/jdk25/bin/javac"
JAVA="$REPO/26.1.2/jdk25/bin/java"

for p in "$JAR" "$SRC" "$JAVAC" "$JAVA"; do
  [ -e "$p" ] || { echo "Missing required path: $p" >&2; exit 1; }
done

mkdir -p "$CLASSES" "$(dirname "$OUT")"
"$JAVAC" -cp "$JAR:$LIBS" -d "$CLASSES" "$SRC"
# JVM deprecation warnings go to stderr; keep them out of the TSV.
"$JAVA" -cp "$CLASSES:$JAR:$LIBS" "$TOOL" "$@" >"$OUT" 2>"$OUT.stderr.log"
echo "Ground truth written: $OUT ($(wc -l <"$OUT") lines)"
