# Raylib AI Coding Guidelines

*This document serves as a "System Prompt" extension for AI agents working on "The Brutalist" or other Raylib projects.*

## 1. Core Philosophy
- **Spartan & Simple**: Avoid over-engineering. Raylib is about getting things on screen quickly. Do not introduce heavy ECS (Entity Component System) frameworks or complex class hierarchies unless strictly necessary.
- **Immediate Mode**: Embrace the `BeginDrawing()` ... `EndDrawing()` loop. Game states should be simple structs or classes updated in a linear flow.
- **No External Dependencies**: "The Brutalist" relies only on Raylib and the C++ Standard Library. Do not suggest adding Boost, SDL, or other libraries.

## 2. Coding Style (C++ Specific)
While Raylib is C99, we are using C++17.
- **Naming Conventions**:
  - **Functions**: `PascalCase` (e.g., `UpdatePlayer`, `GenerateChunk`). Matches Raylib's style.
  - **Variables**: `camelCase` (e.g., `screenWidth`, `playerPos`). Matches Raylib's headers.
  - **Structs/Classes**: `PascalCase`.
- **Math**:
  - Use `raymath.h` for vector operations (e.g., `Vector3Add`, `Vector3Scale`).
  - **Avoid** operator overloading for `Vector3` unless a specific wrapper is used/created. This keeps code portable and readable against Raylib examples.
- **Memory**:
  - Use `std::vector` for dynamic lists (like Chunks, Particles).
  - **Manual Allocation**: If interacting with Raylib's `Mesh.vertices` etc., use `MemAlloc` as Raylib expects raw pointers for `UploadMesh`.

## 3. Architecture Patterns
### The Game Loop
Strictly follow this structure in `main()` or the App class:
```cpp
InitWindow(...);
LoadResources(); // Shaders, Models, Sounds

while (!WindowShouldClose()) {
    Update();
    
    BeginDrawing();
    ClearBackground(...);
    BeginMode3D(camera);
        // Draw 3D World
    EndMode3D();
    // Draw UI
    EndDrawing();
}

UnloadResources();
CloseWindow();
```

### Resource Management
- **RAII is Optional**: For simple Raylib structs (`Shader`, `Model`, `Texture`), strict RAII wrappers can be overkill. Manual `Load...` and `Unload...` is acceptable and often preferred for clarity in this project.
- **Always Unload**: Every `LoadShader` must have a matching `UnloadShader` before `CloseWindow`.

## 4. Specific Raylib Nuances
- **OpenGL Context**: Never call `Load...` functions before `InitWindow`.
- **Shaders**:
  - Always check `shader.id != 0` after loading.
  - Bind attributes if using custom Vertex Shaders ensuring names match (Raylib uses strictly defined attribute locations for `vertexPosition`, `vertexNormal`, etc.).
- **Coordinate System**:
  - **Y-Up**: +Y is Up.
  - **Right-Handed**: Standard OpenGL.

## 5. Debugging & iteration
- Use `TraceLog(LOG_INFO, ...)` for debug output.
- If the screen is blank (white/black), check:
  1. Camera Position/Target (are you looking at the object?).
  2. Shader (fallback to default to verify geometry).
  3. Winding Order (Raylib meshes are CCW by default).

## 6. Project Specifics (The Brutalist)
- **Procedural Gen**: Use deterministic RNG (hashing coordinates) rather than `rand()` where possible for world coherence.
- **Aesthetic**: Focus on Brutalist Architecture—simple geometric shapes, massive scale, concrete textures, fog keying.

---
**Reference**: Always consult `raylib cheatsheet.md` if unsure about an API signature.
