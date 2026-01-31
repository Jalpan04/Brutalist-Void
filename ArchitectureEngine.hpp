#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

// Constants
const float PILLAR_SPACING = 20.0f;
const float PILLAR_WIDTH = 4.0f;
const float CHUNK_SIZE = 400.0f; // 20x20 pillars per chunk
const int PILLARS_PER_AXIS = 20;

// Hash for procedural generation
inline float Hash(int x, int y, int z) {
  int n = x + y * 57 + z * 141;
  n = (n << 13) ^ n;
  return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) /
                     1073741824.0f);
}

// Module Generator Class
class BrutalistEngine {
public:
  struct Chunk {
    Vector3 position;
    Model model;
    std::vector<BoundingBox> colliders;
    bool active;

    void Unload() {
      if (active) {
        UnloadModel(model);
        colliders.clear();
        active = false;
      }
    }
  };

  // Singleton-like helpers or static methods
  static Chunk GenerateChunk(Vector3 chunkPos) {
    Chunk chunk;
    chunk.position = chunkPos;
    chunk.active = true;

    // Verify/Setup mesh building
    // We will manually build vertex arrays to merge meshes
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<unsigned short> indices;
    // Note: Indices limited to 65535, watch out for huge chunks.
    // If chunk is small (5x5 pillars), it's fine. 5x5 = 25 pillars.
    // Each pillar is cube (24 verts). 25*24 = 600 verts. Safe.

    int currentVertexCount = 0;

    auto AddCube = [&](Vector3 pos, Vector3 size) {
      // Add collision
      chunk.colliders.push_back((BoundingBox){
          (Vector3){pos.x - size.x / 2, pos.y - size.y / 2, pos.z - size.z / 2},
          (Vector3){pos.x + size.x / 2, pos.y + size.y / 2,
                    pos.z + size.z / 2}});

      // Generate Cube Vertices
      // We use Raylib's GenMeshCube logic but manually append to vector
      // To simplify, we can use GenMeshCube and extract data, but that's alloc
      // heavy. Better to hardcode cube data.

      float x = size.x / 2;
      float y = size.y / 2;
      float z = size.z / 2;

      // Front face
      // ... (Simple cube geometry generation) ...
      // For brevity in this snippet, implementing a helper:
      Vector3 v[] = {// Front
                     {-x, -y, z},
                     {x, -y, z},
                     {x, y, z},
                     {-x, y, z},
                     // Back
                     {-x, -y, -z},
                     {-x, y, -z},
                     {x, y, -z},
                     {x, -y, -z},
                     // Top
                     {-x, y, -z},
                     {-x, y, z},
                     {x, y, z},
                     {x, y, -z},
                     // Bottom
                     {-x, -y, -z},
                     {x, -y, -z},
                     {x, -y, z},
                     {-x, -y, z},
                     // Right
                     {x, -y, -z},
                     {x, y, -z},
                     {x, y, z},
                     {x, -y, z},
                     // Left
                     {-x, -y, -z},
                     {-x, -y, z},
                     {-x, y, z},
                     {-x, y, -z}};

      Vector3 n[] = {{0, 0, 1},  {0, 0, 1},  {0, 0, 1},  {0, 0, 1},  {0, 0, -1},
                     {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 1, 0},  {0, 1, 0},
                     {0, 1, 0},  {0, 1, 0},  {0, -1, 0}, {0, -1, 0}, {0, -1, 0},
                     {0, -1, 0}, {1, 0, 0},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},
                     {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}};

      // Standard cube indices (0,1,2, 0,2,3)
      int ind[] = {0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,
                   8,  9,  10, 8,  10, 11, 12, 13, 14, 12, 14, 15,
                   16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

      for (int i = 0; i < 24; i++) {
        vertices.push_back(Vector3Add(v[i], pos));
        normals.push_back(n[i]);
        texcoords.push_back((Vector2){
            0, 0}); // UVs not critical for proc shader but required for Mesh
      }

      for (int i = 0; i < 36; i++) {
        indices.push_back(currentVertexCount + ind[i]);
      }
      currentVertexCount += 24;
    };

    // Grid generation logic
    // Grid generation logic
    int startX = (int)chunkPos.x;
    int startZ = (int)chunkPos.z;

    // --- SCIENTIFIC GENERATION: BINARY SPACE PARTITIONING (BSP) ---
    // Inspired by "Algorithmic Beauty of Buildings"
    // 1. Define the Scope (Root Volume)
    struct Rect {
      float x, z, w, h;
    };
    std::vector<Rect> blocks;

    // Recursive Split Lambda
    std::function<void(Rect, int)> RecursiveSplit = [&](Rect r, int depth) {
      // Stop constraints
      if (depth <= 0 || r.w < 30.0f || r.h < 30.0f) {
        blocks.push_back(r);
        return;
      }

      // Deterministic Split using Center Hash
      float cx = r.x + r.w / 2 + chunkPos.x;
      float cz = r.z + r.h / 2 + chunkPos.z;
      float hSplit = Hash((int)cx, (int)cz, depth);

      bool splitX = r.w > r.h;
      if (abs(r.w - r.h) < 10.0f)
        splitX = hSplit > 0.5f;

      // Golden Mean Ratio (Scientific Division)
      float ratio = 0.38f + (hSplit * 0.24f);
      float streetGap = 6.0f; // Defined street width

      if (splitX) {
        float w1 = r.w * ratio;
        float w2 = r.w * (1.0f - ratio);
        if (w1 < 20 || w2 < 20) {
          blocks.push_back(r);
          return;
        } // Too small to split
        RecursiveSplit({r.x, r.z, w1 - streetGap / 2, r.h}, depth - 1);
        RecursiveSplit({r.x + w1 + streetGap / 2, r.z, w2 - streetGap / 2, r.h},
                       depth - 1);
      } else {
        float h1 = r.h * ratio;
        float h2 = r.h * (1.0f - ratio);
        if (h1 < 20 || h2 < 20) {
          blocks.push_back(r);
          return;
        }
        RecursiveSplit({r.x, r.z, r.w, h1 - streetGap / 2}, depth - 1);
        RecursiveSplit({r.x, r.z + h1 + streetGap / 2, r.w, h2 - streetGap / 2},
                       depth - 1);
      }
    };

    // Start Split (Root covers 400x400)
    // Offset to center (Chunk is -200 to +200 relative to center)
    RecursiveSplit({-200.0f, -200.0f, 400.0f, 400.0f}, 6);

    // 2. Render Architectures for each Block "Leaf"
    for (const auto &b : blocks) {
      float cx = b.x + b.w / 2 + chunkPos.x;
      float cz = b.z + b.h / 2 + chunkPos.z;

      // Spawn Safety
      if (sqrt(cx * cx + cz * cz) < 25.0f)
        continue;

      // Hash for Block Identity
      float hBlock = Hash((int)cx, 42, (int)cz);
      float baseHeight = 20.0f + (hBlock * 100.0f);
      float hType = Hash((int)cx, 55, (int)cz);

      // S. The Giant Statue (Rare Totems)
      if (hType > 0.92f && b.w > 20 && b.h > 20) {
        float statueH = baseHeight * 1.5f;
        // Base/Legs
        AddCube((Vector3){cx, statueH * 0.2f, cz},
                (Vector3){b.w * 0.4f, statueH * 0.4f, b.h * 0.4f});
        // Torso
        AddCube((Vector3){cx, statueH * 0.6f, cz},
                (Vector3){b.w * 0.25f, statueH * 0.4f, b.h * 0.25f});
        // Head (Abstract/Offset)
        AddCube((Vector3){cx, statueH * 0.9f, cz + b.h * 0.05f},
                (Vector3){b.w * 0.2f, statueH * 0.2f, b.h * 0.3f});

        // "Wires" hanging from statue
        AddCube((Vector3){cx + b.w * 0.15f, statueH * 0.8f, cz},
                (Vector3){0.1f, statueH * 0.5f, 0.1f});
        continue;
      }

      // A. The Citadel (Large Monolithic Blocks)
      if (b.w > 60.0f && b.h > 60.0f) {
        // Main Mass
        AddCube((Vector3){b.x + b.w / 2 + chunkPos.x, baseHeight / 2,
                          b.z + b.h / 2 + chunkPos.z},
                (Vector3){b.w, baseHeight, b.h});

        // Detail: Recessed Top
        AddCube((Vector3){b.x + b.w / 2 + chunkPos.x, baseHeight + 5.0f,
                          b.z + b.h / 2 + chunkPos.z},
                (Vector3){b.w * 0.6f, 10.0f, b.h * 0.6f});
        continue;
      }

      // B. The Grid (Pillars within Block)
      // hType already accepted for Statue check
      if (hType > 0.4f) {
        int cols = (int)(b.w / 12.0f);
        int rows = (int)(b.h / 12.0f);
        if (cols == 0)
          cols = 1;
        if (rows == 0)
          rows = 1;

        float sx = b.w / cols;
        float sz = b.h / rows;

        for (int i = 0; i < cols; i++) {
          for (int j = 0; j < rows; j++) {
            Vector3 p = {b.x + chunkPos.x + i * sx + sx / 2,
                         0, // calculated below
                         b.z + chunkPos.z + j * sz + sz / 2};
            float pHeight =
                baseHeight * (0.8f + Hash((int)p.x, 1, (int)p.z) * 0.4f);
            p.y = pHeight / 2;

            AddCube(p, (Vector3){4.0f, pHeight, 4.0f});

            // Streets in the Sky (Block Internal)
            if (Hash((int)p.x, 9, (int)p.z) > 0.7f && i < cols - 1) {
              AddCube((Vector3){p.x + sx / 2, pHeight - 4.0f, p.z},
                      (Vector3){sx, 1.5f, 5.0f});
            }
          }
        }
        continue;
      }

      // C. Fragmentation (Stairs/Plaza)
      if (hType < 0.2f) {
        // Stairs
        int steps = 15;
        float sh = 0.5f; // Walkable
        for (int s = 0; s < steps; s++) {
          AddCube((Vector3){cx, s * sh + sh / 2, cz},
                  (Vector3){b.w, sh, b.h - s * (b.h / steps)});
        }
        continue;
      }

      // D. Slab (Default)
      AddCube((Vector3){cx, baseHeight / 4, cz},
              (Vector3){b.w, baseHeight / 2, b.h});

      // W. "The Wires" (Chaotic Cables)
      // Dangle from the structures we just made
      float hWire = Hash((int)cx, 99, (int)cz);
      if (hWire > 0.5f) {
        int cableCount = (int)(hWire * 5.0f); // 0 to 5 cables
        for (int k = 0; k < cableCount; k++) {
          // Random position on the block edges or center
          float wx = cx + (Hash((int)cx, k, 100) - 0.5f) * b.w;
          float wz = cz + (Hash((int)cz, k, 200) - 0.5f) * b.h;
          float wy =
              baseHeight * (0.8f + Hash((int)k, 1, 300) * 0.2f); // High up
          float len = 15.0f + Hash((int)wx, (int)wz, k) * 40.0f; // Long cables

          // Thin black line
          AddCube((Vector3){wx, wy - len / 2, wz},
                  (Vector3){0.15f, len, 0.15f});

          // Cross-wire (connecting to nowhere?)
          if (k % 2 == 0) {
            AddCube((Vector3){wx, wy - len * 0.2f, wz},
                    (Vector3){len * 0.5f, 0.1f, 0.1f});
          }
        }
      }
    }

    // Construct Mesh
    Mesh mesh = {0};
    mesh.vertexCount = (int)vertices.size();
    mesh.triangleCount = (int)indices.size() / 3;

    mesh.vertices = (float *)MemAlloc(vertices.size() * 3 * sizeof(float));
    mesh.normals = (float *)MemAlloc(normals.size() * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(texcoords.size() * 2 * sizeof(float));
    mesh.indices =
        (unsigned short *)MemAlloc(indices.size() * sizeof(unsigned short));

    // Copy data
    for (int i = 0; i < vertices.size(); i++) {
      mesh.vertices[i * 3] = vertices[i].x;
      mesh.vertices[i * 3 + 1] = vertices[i].y;
      mesh.vertices[i * 3 + 2] = vertices[i].z;

      mesh.normals[i * 3] = normals[i].x;
      mesh.normals[i * 3 + 1] = normals[i].y;
      mesh.normals[i * 3 + 2] = normals[i].z;

      mesh.texcoords[i * 2] = texcoords[i].x;
      mesh.texcoords[i * 2 + 1] = texcoords[i].y;
    }
    for (int i = 0; i < indices.size(); i++) {
      mesh.indices[i] = indices[i];
    }

    UploadMesh(&mesh, false);
    chunk.model = LoadModelFromMesh(mesh);
    return chunk;
  }
};
