#pragma once

// 1:1 port of the private enum TABLES of net.minecraft.client.renderer.block.BlockModelLighter
// (BlockModelLighter.java:284-624, 696-716) — the smooth-lighting (ambient occlusion) geometry
// data: SizeInfo (faceShape index ordering), AdjacencyInfo (per-face neighbor corner Directions +
// the 8 per-vertex faceShape-weight indices), and AmbientVertexRemap (output vertex reorder per
// facing). These are the foundation the AO blend (prepareQuadAmbientOcclusion) indexes into; this
// header certifies the transcription before the blend is ported. Certified by
// block_model_lighter_tables_parity (reflection-dumped vs the real enums).
//
// All enums are indexed by Direction.get3DDataValue(), which equals the ordinal for DOWN..EAST
// (DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5). NOTE: the AdjacencyInfo constructor's shadeWeight
// arg (0.5/1.0/0.8/0.8/0.6/0.6) is NOT stored (dead — CardinalLighting now supplies face shade).

#include <array>

namespace mc::render::block {

namespace aolight {

// SizeInfo (BlockModelLighter.java:696-716): faceShape[] slot index (== ordinal here).
enum SizeIdx {
    S_DOWN = 0, S_UP = 1, S_NORTH = 2, S_SOUTH = 3, S_WEST = 4, S_EAST = 5,
    S_FLIP_DOWN = 6, S_FLIP_UP = 7, S_FLIP_NORTH = 8, S_FLIP_SOUTH = 9, S_FLIP_WEST = 10, S_FLIP_EAST = 11,
};
inline constexpr int SIZE_INFO_COUNT = 12;

// AdjacencyInfo (BlockModelLighter.java:284-591) per Direction ordinal.
struct AdjacencyInfo {
    int corners[4];           // neighbor Direction ordinals
    bool doNonCubicWeight;
    int vertWeights[4][8];    // [vertex 0..3][8 SizeInfo indices] (pairwise products in the blend)
};

inline constexpr AdjacencyInfo ADJACENCY[6] = {
    // DOWN — corners W,E,N,S
    {{4, 5, 2, 3}, true, {
        {10, 3, 10, 9, 4, 9, 4, 3},
        {10, 2, 10, 8, 4, 8, 4, 2},
        {11, 2, 11, 8, 5, 8, 5, 2},
        {11, 3, 11, 9, 5, 9, 5, 3}}},
    // UP — corners E,W,N,S
    {{5, 4, 2, 3}, true, {
        {5, 3, 5, 9, 11, 9, 11, 3},
        {5, 2, 5, 8, 11, 8, 11, 2},
        {4, 2, 4, 8, 10, 8, 10, 2},
        {4, 3, 4, 9, 10, 9, 10, 3}}},
    // NORTH — corners U,D,E,W
    {{1, 0, 5, 4}, true, {
        {1, 10, 1, 4, 7, 4, 7, 10},
        {1, 11, 1, 5, 7, 5, 7, 11},
        {0, 11, 0, 5, 6, 5, 6, 11},
        {0, 10, 0, 4, 6, 4, 6, 10}}},
    // SOUTH — corners W,E,D,U
    {{4, 5, 0, 1}, true, {
        {1, 10, 7, 10, 7, 4, 1, 4},
        {0, 10, 6, 10, 6, 4, 0, 4},
        {0, 11, 6, 11, 6, 5, 0, 5},
        {1, 11, 7, 11, 7, 5, 1, 5}}},
    // WEST — corners U,D,N,S
    {{1, 0, 2, 3}, true, {
        {1, 3, 1, 9, 7, 9, 7, 3},
        {1, 2, 1, 8, 7, 8, 7, 2},
        {0, 2, 0, 8, 6, 8, 6, 2},
        {0, 3, 0, 9, 6, 9, 6, 3}}},
    // EAST — corners D,U,N,S
    {{0, 1, 2, 3}, true, {
        {6, 3, 6, 9, 0, 9, 0, 3},
        {6, 2, 6, 8, 0, 8, 0, 2},
        {7, 2, 7, 8, 1, 8, 1, 2},
        {7, 3, 7, 9, 1, 9, 1, 3}}},
};

// AmbientVertexRemap (BlockModelLighter.java:593-624) per Direction ordinal: {vert0..vert3}.
inline constexpr int AMBIENT_VERTEX_REMAP[6][4] = {
    {0, 1, 2, 3},  // DOWN
    {2, 3, 0, 1},  // UP
    {3, 0, 1, 2},  // NORTH
    {0, 1, 2, 3},  // SOUTH
    {3, 0, 1, 2},  // WEST
    {1, 2, 3, 0},  // EAST
};

}  // namespace aolight

}  // namespace mc::render::block
