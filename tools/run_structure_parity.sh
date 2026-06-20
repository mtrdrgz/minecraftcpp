#!/usr/bin/env bash
# run_structure_parity.sh — runs every registered structure parity target
# end-to-end: Java ground-truth TSV -> C++ parity test -> pass/fail summary.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO/build"
TOOLS="$REPO/tools"
GT="$TOOLS/run_groundtruth.sh"

# Map: C++ target name | Java tool name | output TSV name (relative to build/)
# Tests that are pure self-checks (no Java GT) are listed with empty Java tool.
TARGETS=(
  "woodland_mansion_grid_parity|WoodlandMansionGridParity|woodland_mansion_grid.tsv"
  "ocean_monument_room_parity|OceanMonumentRoomParity|ocean_monument_room.tsv"
  "bounding_box_parity|BoundingBoxParity|bounding_box.tsv"
  "jigsaw_height_limit_parity|JigsawHeightLimitParity|jigsaw_height_limit.tsv"
  "ocean_monument_room_graph_parity|OceanMonumentRoomGraphParity|ocean_monument_room_graph.tsv"
  "linear_pos_test_math_parity|LinearPosTestMathParity|linear_pos_test_math.tsv"
  "igloo_piece_position_parity|IglooPiecePositionParity|igloo_piece_position.tsv"
  "woodland_mansion_edge_clean_parity|WoodlandMansionEdgeCleanParity|woodland_mansion_edge_clean.tsv"
  "ocean_monument_room_fitter_parity|OceanMonumentRoomFitterParity|ocean_monument_room_fitter.tsv"
  "stronghold_piece_box_parity|StrongholdPieceBoxParity|stronghold_piece_box.tsv"
  "nether_fortress_piece_box_parity|NetherFortressPieceBoxParity|nether_fortress_piece_box.tsv"
  "stronghold_piece_type_box_parity|StrongholdPieceTypeBoxParity|stronghold_piece_type_box.tsv"
  "bounding_box_aggregate_parity|BoundingBoxAggregateParity|bounding_box_aggregate.tsv"
  "ocean_ruin_cluster_parity|OceanRuinClusterParity|ocean_ruin_cluster.tsv"
  "structure_pieces_builder_math_parity|StructurePiecesBuilderMathParity|structure_pieces_builder_math.tsv"
  "concentric_rings_parity|ConcentricRingsPositionsParity|concentric_rings_positions.tsv"
  "random_block_match_test_parity||"
  "processorrule_parity||"
  "scattered_feature_box_parity|ScatteredFeatureBoxParity|scattered_feature_box.tsv"
  "jungle_temple_stone_selector_parity||"
  "mine_shaft_corridor_parity|MineShaftCorridorParity|mine_shaft_corridor.tsv"
  "smooth_stone_selector_parity|SmoothStoneSelectorParity|smooth_stone_selector.tsv"
  "mine_shaft_crossing_box_parity|MineShaftCrossingBoxParity|mine_shaft_crossing_box.tsv"
  "mine_shaft_room_box_parity|MineShaftRoomBoxParity|mine_shaft_room_box.tsv"
  "woodland_mansion_grid_layout_parity|WoodlandMansionGridLayoutParity|woodland_mansion_grid_layout.tsv"
  "structure_template_build_info_list_parity|StructureTemplateBuildInfoListParity|structure_template_build_info_list.tsv"
  "stronghold_small_door_parity|StrongholdSmallDoorParity|stronghold_small_door.tsv"
  "ruined_portal_yselector_parity|RuinedPortalYSelectorParity|ruined_portal_yselector.tsv"
  "nether_fortress_child_offset_parity|NetherFortressChildOffsetParity|nether_fortress_child_offset.tsv"
  "structure_template_loader_parity|StructureTemplateLoaderParity|structure_template_loader.tsv"
  "structure_template_pool_parity|StructureTemplatePoolParity|structure_template_pool.tsv"
  "jigsaw_attach_parity|JigsawAttachParity|jigsaw_attach.tsv"
  "jigsaw_placement_parity|JigsawPlacementParity|jigsaw_placement.tsv"
  "structure_piece_math_parity|StructurePieceMathParity|structure_piece_math.tsv"
  "structure_connected_position_parity|StructureConnectedPositionParity|structure_connected_position.tsv"
  "structure_piece_collection_parity|StructurePieceCollectionParity|structure_piece_collection.tsv"
  "axis_aligned_linear_pos_predicate_parity|AxisAlignedLinearPosTestPredicateParity|axis_aligned_linear_pos_predicate.tsv"
  "mineshaft_stairs_box_parity|MineshaftStairsBoxParity|mineshaft_stairs_box.tsv"
  "structure_transform_parity|StructureTransformsParity|structure_transforms.tsv"
  "structure_placeinworld_parity|StructurePlaceInWorldParity|structure_placeinworld.tsv"
  "structure_processor_parity|StructureProcessorParity|structure_processor.tsv"
  "swamp_hut_piece_parity|SwampHutPieceParity|swamp_hut_piece.tsv"
)

PASS=0
FAIL=0
SKIP=0
FAIL_NAMES=()

for entry in "${TARGETS[@]}"; do
  IFS='|' read -r tgt jtool tsv <<<"$entry"
  if [ ! -x "$BUILD/$tgt" ]; then
    echo "MISS $tgt  (binary not built)"
    SKIP=$((SKIP+1))
    continue
  fi
  if [ -n "$jtool" ]; then
    if ! bash "$GT" "$jtool" "$BUILD/$tsv" >/dev/null 2>"$BUILD/$tsv.stderr.log"; then
      echo "SKIP  $tgt  (Java ground-truth failed; see $BUILD/$tsv.stderr.log)"
      SKIP=$((SKIP+1))
      continue
    fi
    # Tests that need extra arg files (--states, --tags, --fam) get them here.
    case "$tgt" in
      structure_processor_parity)
        OUT=$(cd "$REPO" && "$BUILD/$tgt" --cases "$BUILD/$tsv" \
              --states "$REPO/src/assets/block_states.json" \
              --tags "$REPO/26.1.2/data/minecraft/tags/block" 2>&1 | tail -3 | tr '\n' '|')
        ;;
      structure_placeinworld_parity|swamp_hut_piece_parity)
        # Both need block_rotate_mirror.tsv (FAM) which is produced by
        # block_rotate_mirror_parity's own Java GT. Generate it on demand.
        if [ ! -f "$BUILD/block_rotate_mirror.tsv" ]; then
          if ! bash "$GT" BlockRotateMirrorParity "$BUILD/block_rotate_mirror.tsv" >/dev/null 2>"$BUILD/block_rotate_mirror.tsv.stderr.log"; then
            echo "SKIP  $tgt  (block_rotate_mirror GT failed)"
            SKIP=$((SKIP+1))
            continue
          fi
        fi
        OUT=$(cd "$REPO" && "$BUILD/$tgt" --cases "$BUILD/$tsv" \
              --states "$REPO/src/assets/block_states.json" \
              --fam "$BUILD/block_rotate_mirror.tsv" 2>&1 | tail -3 | tr '\n' '|')
        ;;
      *)
        OUT=$(cd "$REPO" && "$BUILD/$tgt" --cases "$BUILD/$tsv" 2>&1 | tail -3 | tr '\n' '|')
        ;;
    esac
  else
    OUT=$("$BUILD/$tgt" 2>&1 | tail -3 | tr '\n' '|')
  fi
  if echo "$OUT" | grep -qE "(mismatches=0|placeMism=0 countMism=0)"; then
    echo "PASS  $tgt  $OUT"
    PASS=$((PASS+1))
  else
    echo "FAIL  $tgt  $OUT"
    FAIL=$((FAIL+1))
    FAIL_NAMES+=("$tgt")
  fi
done

echo ""
echo "==============================="
echo "PASS=$PASS  FAIL=$FAIL  SKIP=$SKIP"
[ $FAIL -gt 0 ] && printf 'Failed: %s\n' "${FAIL_NAMES[@]}"
