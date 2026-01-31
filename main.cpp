#include "ArchitectureEngine.hpp"
#include "raylib.h"
#include "raymath.h"
#include <cstdio> // For _popen
#include <iostream>
#include <string>
#include <vector>

#define MAX_CHUNKS_X 4
#define MAX_CHUNKS_Z 4

// Movement constants
#define GRAVITY 32.0f
#define MAX_SPEED 14.0f
#define JUMP_FORCE 10.0f
#define FRICTION 0.90f
#define AIR_DRAG 0.98f
#define MOUSE_SENSITIVITY 0.003f

// Custom Camera State
struct Player {
  Vector3 position;
  Vector3 velocity;
  Camera3D camera;
  float pitch;
  float yaw;
  bool isGrounded;
  float headBobTimer;
  float smoothY; // For visual smoothing of stairs

  // Auto-Pilot State
  bool autoPilot;
  float autoTurnTarget; // Target Yaw
  float autoTurnTimer;
};

// Simple audio callback for brown/white noise
void NoiseCallback(void *buffer, unsigned int frames) {
  short *d = (short *)buffer;
  static float last = 0.0f;
  for (unsigned int i = 0; i < frames; i++) {
    float white = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    last = (last + white * 0.1f) * 0.95f; // Brown-ish integration
    if (last > 1.0f)
      last = 1.0f;
    if (last < -1.0f)
      last = -1.0f;
    d[i] = (short)(last * 2000.0f); // Low volume
  }
}

bool CheckCollision(Vector3 position, float radius, float height,
                    const std::vector<BrutalistEngine::Chunk> &chunks) {
  BoundingBox playerBox = {
      (Vector3){position.x - radius, position.y - height, position.z - radius},
      (Vector3){position.x + radius, position.y, position.z + radius}};

  for (const auto &chunk : chunks) {
    if (Vector3Distance(chunk.position, position) > 300.0f)
      continue;
    for (const auto &box : chunk.colliders) {
      if (CheckCollisionBoxes(playerBox, box))
        return true;
    }
  }
  return false;
}

void UpdatePlayer(Player *player,
                  const std::vector<BrutalistEngine::Chunk> &chunks, float dt) {
  // 1. Input
  Vector2 input = {0};

  // --- CINEMATIC AUTO-PILOT ---
  if (player->autoPilot) {
    input.y = 0.5f; // Slow walk forward

    // Look around slowly (Cinematic Pan)
    float time = (float)GetTime();
    player->pitch = sin(time * 0.2f) * 0.15f;
    // player->yaw += sin(time * 0.1f) * 0.003f; // Gentle drift

    // Collision Avoidance / Steering
    Vector3 forward = {cos(player->yaw), 0, sin(player->yaw)};
    Vector3 checkPos =
        Vector3Add(player->position, Vector3Scale(forward, 3.0f));

    if (CheckCollision(checkPos, 0.5f, 1.0f, chunks)) {
      // Wall ahead! Turn away
      player->autoTurnTarget += dt * 2.0f; // Spin right
    }

    // Smooth steer towards target yaw
    player->yaw = Lerp(player->yaw, player->autoTurnTarget, dt * 1.0f);

  } else {
    // Manual Input
    if (IsKeyDown(KEY_W))
      input.y += 1;
    if (IsKeyDown(KEY_S))
      input.y -= 1;
    if (IsKeyDown(KEY_A))
      input.x -= 1;
    if (IsKeyDown(KEY_D))
      input.x += 1;
  }

  if (Vector2Length(input) > 0)
    input = Vector2Normalize(input);

  // 2. Look
  Vector2 mouseDelta = GetMouseDelta();
  player->yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
  player->pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
  player->pitch = Clamp(player->pitch, -1.5f, 1.5f);

  // 3. Physics (Gravity)
  if (!player->isGrounded)
    player->velocity.y -= GRAVITY * dt;

  // Jump
  if (player->isGrounded && IsKeyPressed(KEY_SPACE)) {
    player->velocity.y = JUMP_FORCE;
    player->isGrounded = false;
  }

  // 4. Movement Calculation
  Vector3 forward = {sinf(player->yaw), 0, cosf(player->yaw)};
  Vector3 right = {cosf(player->yaw), 0, -sinf(player->yaw)};

  Vector3 moveDir =
      Vector3Add(Vector3Scale(forward, input.y), Vector3Scale(right, input.x));
  float currentSpeed = MAX_SPEED;
  if (IsKeyDown(KEY_LEFT_CONTROL))
    currentSpeed *= 0.5f; // Crouch (slow)
  if (IsKeyDown(KEY_LEFT_SHIFT))
    currentSpeed *= 1.5f; // Sprint

  Vector3 targetVel = Vector3Scale(moveDir, currentSpeed);

  // Friction/Smoothing
  float friction = player->isGrounded ? FRICTION : AIR_DRAG;
  // Simple Lerp for horizontal velocity
  player->velocity.x =
      Lerp(player->velocity.x, targetVel.x, (1.0f - friction) * 15.0f * dt);
  player->velocity.z =
      Lerp(player->velocity.z, targetVel.z, (1.0f - friction) * 15.0f * dt);

  // 5. Integration with Collision (Sliding & Auto-Step)
  float stepHeight = 0.6f; // Can step up 0.6 units (stairs are 0.5)

  // X-Axis
  player->position.x += player->velocity.x * dt;
  if (CheckCollision(player->position, 0.3f, 1.8f, chunks)) {
    // Try Auto-Step
    bool stepped = false;
    if (player->isGrounded) {
      Vector3 testPos = player->position;
      testPos.y += stepHeight;
      if (!CheckCollision(testPos, 0.3f, 1.8f, chunks)) {
        player->position.y += stepHeight;
        stepped = true;
      }
    }

    if (!stepped) {
      player->position.x -= player->velocity.x * dt;
      player->velocity.x = 0;
    }
  }
  // Z-Axis
  player->position.z += player->velocity.z * dt;
  if (CheckCollision(player->position, 0.3f, 1.8f, chunks)) {
    // Try Auto-Step
    bool stepped = false;
    if (player->isGrounded) {
      Vector3 testPos = player->position;
      testPos.y += stepHeight;
      if (!CheckCollision(testPos, 0.3f, 1.8f, chunks)) {
        player->position.y += stepHeight;
        stepped = true;
      }
    }

    if (!stepped) {
      player->position.z -= player->velocity.z * dt;
      player->velocity.z = 0;
    }
  }
  // Y-Axis
  player->position.y += player->velocity.y * dt;

  // Check Ceiling/Pillars Y collision
  if (CheckCollision(player->position, 0.3f, 1.8f, chunks)) {
    // If moving up (jump into ceiling)
    if (player->velocity.y > 0) {
      player->position.y -= player->velocity.y * dt;
      player->velocity.y = 0;
    }
    // If moving down (hitting top of pillar?)
    else if (player->velocity.y < 0) {
      player->position.y -= player->velocity.y * dt;
      player->velocity.y = 0;
      player->isGrounded = true;
    }
  }

  // Ground Check (Floor at Y=0.0f)
  // Player position roughly represents "Eyes/Head" based on collision box logic
  // (pos.y - height)
  float playerHeight = 1.8f;
  if (IsKeyDown(KEY_LEFT_CONTROL))
    playerHeight = 1.0f; // Crouching height

  if (player->position.y <= playerHeight) {
    player->position.y = playerHeight;
    if (player->velocity.y < 0)
      player->velocity.y = 0;
    player->isGrounded = true;
  } else {
    // Check for ground below
    // Probe slightly down to see if we are standing on something
    Vector3 probe = player->position;
    probe.y -= 0.1f; // Check 10cm below

    // We need to only check feet collision, not full body, theoretically.
    // But CheckCollision checks a full box.
    // If we move down 0.1f and collide, we are effectively grounded.
    if (CheckCollision(probe, 0.3f, 1.8f, chunks)) {
      player->isGrounded = true;
      if (player->velocity.y < 0)
        player->velocity.y = 0;

      // Optional: Snap to surface to prevent jitter?
      // For now, relies on gravity pushing down and collision pushing up in
      // next frame? Actually, Y-Axis collision earlier (lines 159-170) handles
      // the "push up". So here we just set the flag.
    } else {
      player->isGrounded = false;
    }
  }

  // Head Bob
  if (player->isGrounded && Vector2Length(input) > 0.1f) {
    player->headBobTimer += dt * 12.0f;
  } else {
    player->headBobTimer = 0.0f;
  }

  // Camera Update
  float bobOffset = sinf(player->headBobTimer) * 0.1f;
  float eyeHeight = 1.6f;
  if (IsKeyDown(KEY_LEFT_CONTROL))
    eyeHeight = 0.8f; // Crouch height

  // Lerp eye height?
  // Kept simple for now.

  // Camera Smoothing
  // Lerp smoothY towards physical Y to hide the snap
  player->smoothY = Lerp(player->smoothY, player->position.y, 15.0f * dt);

  Vector3 camPos = player->position;
  camPos.y = player->smoothY; // Use smoothed Y

  player->camera.position =
      (Vector3){camPos.x, camPos.y + eyeHeight + bobOffset, camPos.z};

  Vector3 camForward = {sinf(player->yaw) * cosf(player->pitch),
                        sinf(player->pitch),
                        cosf(player->yaw) * cosf(player->pitch)};
  player->camera.target = Vector3Add(player->camera.position, camForward);
}

int main() {
  // 1. Initialization
  InitWindow(1280, 720, "Brutalist Void - Procedural Infinite Architecture");
  InitAudioDevice();
  SetTargetFPS(60);
  DisableCursor();

  // 2. Audio Setup
  AudioStream voidHum = LoadAudioStream(44100, 16, 1);
  SetAudioStreamCallback(voidHum, NoiseCallback);
  PlayAudioStream(voidHum);

  // 3. Player/Camera Setup
  Player player = {0};
  player.position = (Vector3){0.0f, 1.8f, 0.0f}; // Start on head height
  player.velocity = (Vector3){0.0f, 0.0f, 0.0f};
  player.isGrounded = false;
  player.smoothY = 1.8f; // Init smoothY
  player.camera.position = player.position;
  player.camera.target = (Vector3){0.0f, 1.8f, 1.0f};
  player.camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  player.camera.fovy = 60.0f;
  player.camera.projection = CAMERA_PERSPECTIVE;
  player.yaw = 90.0f * DEG2RAD;
  player.yaw = 90.0f * DEG2RAD;
  player.pitch = 0.0f;

  // Auto-Pilot Init
  player.autoPilot = false;
  player.autoTurnTarget = player.yaw;
  player.autoTurnTimer = 0;

  // Recording State
  FILE *ffmpegPipe = nullptr;

  // 4. Load Shader
  // 4. Load Shader
  // Try multiple paths
  Shader concreteShader = LoadShader("Shaders/procedural_concrete.vs",
                                     "Shaders/procedural_concrete.fs");
  if (concreteShader.id == 0) {
    TraceLog(LOG_WARNING, "Trying ../Shaders path...");
    concreteShader = LoadShader("../Shaders/procedural_concrete.vs",
                                "../Shaders/procedural_concrete.fs");
  }

  // Check if loaded
  if (concreteShader.id == 0) {
    TraceLog(LOG_ERROR, "CRITICAL: Shader failed to load! Check console.");
  } else {
    TraceLog(LOG_INFO, "Shader Loaded Successfully ID: %i", concreteShader.id);
  }

  int lightDirLoc = GetShaderLocation(concreteShader, "lightDir");
  int viewPosLoc = GetShaderLocation(concreteShader, "viewPos");
  int timeLoc = GetShaderLocation(concreteShader, "time");
  int creepyModeLoc = GetShaderLocation(concreteShader, "creepyMode");
  int lightColorLoc = GetShaderLocation(concreteShader, "lightColor");
  int ambientColorLoc = GetShaderLocation(concreteShader, "ambientColor");

  // High contrast light
  Vector3 lightDir = Vector3Normalize((Vector3){0.5f, -1.0f, 0.5f});
  SetShaderValue(concreteShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);

  // Set Colors (White, shader handles intensity)
  Vector4 colorWhite = {1.0f, 1.0f, 1.0f, 1.0f};
  SetShaderValue(concreteShader, lightColorLoc, &colorWhite,
                 SHADER_UNIFORM_VEC4);
  SetShaderValue(concreteShader, ambientColorLoc, &colorWhite,
                 SHADER_UNIFORM_VEC4);

  // Lighting mode toggle (Ctrl to switch)
  bool creepyMode = true; // Start in creepy mode
  int creepyModeInt = 1;
  SetShaderValue(concreteShader, creepyModeLoc, &creepyModeInt,
                 SHADER_UNIFORM_INT);

  // 5. Generate World
  std::vector<BrutalistEngine::Chunk> chunks;
  float chunkWorldSize = 20 * 20.0f; // Match PILLARS_PER_AXIS * PILLAR_SPACING

  // Create 5x5 grid (100x100 pillars total) centered roughly on origin
  for (int x = -2; x <= 2; x++) {
    for (int z = -2; z <= 2; z++) {
      chunks.push_back(BrutalistEngine::GenerateChunk(
          (Vector3){x * chunkWorldSize, 0.0f, z * chunkWorldSize}));
      // Apply shader
      for (int m = 0; m < chunks.back().model.meshCount; m++) {
        chunks.back().model.materials[m].shader = concreteShader;
      }
    }
  }

  // Main Loop
  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    float time = (float)GetTime();

    // --- UPDATE ---

    // Toggle lighting mode with Ctrl
    if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) {
      creepyMode = !creepyMode;
      creepyModeInt = creepyMode ? 1 : 0;
      SetShaderValue(concreteShader, creepyModeLoc, &creepyModeInt,
                     SHADER_UNIFORM_INT);
      TraceLog(LOG_INFO, "Lighting Mode: %s",
               creepyMode ? "CREEPY" : "LIMINAL");
    }

    // Toggle Fullscreen (F11)
    if (IsKeyPressed(KEY_F11)) {
      ToggleFullscreen();
    }

    // Toggle Auto-Pilot (P)
    if (IsKeyPressed(KEY_P)) {
      player.autoPilot = !player.autoPilot;
      player.autoTurnTarget = player.yaw;
      TraceLog(LOG_INFO, "Auto-Pilot: %s", player.autoPilot ? "ON" : "OFF");
    }

    // Toggle Recording (R)
    if (IsKeyPressed(KEY_R)) {
      if (ffmpegPipe) {
        _pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
        TraceLog(LOG_INFO, "RECORDING STOPPED (Saved to recording.mp4)");
      } else {
        // ffmpeg command to read raw RGBA from stdin with DYNAMIC resolution
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -f rawvideo -pixel_format rgba -video_size %dx%d "
                 "-framerate 60 -i - -c:v libx264 -preset ultrafast -y "
                 "recording.mp4",
                 w, h);

        ffmpegPipe = _popen(cmd, "wb");
        if (!ffmpegPipe)
          TraceLog(LOG_ERROR, "FAILED TO START FFMPEG RECORDING");
        else
          TraceLog(LOG_INFO, "RECORDING STARTED at %dx%d...", w, h);
      }
    }

    UpdatePlayer(&player, chunks, dt);

    // "The Fall" Loop
    if (player.position.y < -30.0f) {
      player.position = (Vector3){player.position.x, 60.0f, player.position.z};
      player.velocity = (Vector3){0};
    }

    // Shader Updates
    SetShaderValue(concreteShader, viewPosLoc, &player.camera.position,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(concreteShader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
    // UpdateAudioStream(voidHum); // Not needed with SetAudioStreamCallback

    // --- DRAW ---
    BeginDrawing();
    if (creepyMode) {
      ClearBackground(
          (Color){13, 13, 15, 255}); // Match Creepy Fog (Deep dark blue-grey)
    } else {
      ClearBackground(
          (Color){51, 51, 51, 255}); // Match Liminal Fog (Medium Grey)
    }

    BeginMode3D(player.camera);

    // DRAW FLOOR
    // Massive dark floor to provide perspective/horizon
    DrawPlane((Vector3){0.0f, 0.0f, 0.0f}, (Vector2){5000.0f, 5000.0f},
              (Color){20, 20, 20, 255});

    for (auto &chunk : chunks) {
      DrawModel(chunk.model, (Vector3){0, 0, 0}, 1.0f, WHITE);
      // Draw Wireframe overlay for "Grid" aesthetic?
      // Optional: DrawModelWires(chunk.model, (Vector3){0,0,0}, 1.0f,
      // (Color){0,0,0,50});
    }
    EndMode3D();

    // UI
    DrawFPS(10, 10);

    // Vignette or Cinematics could go here

    EndDrawing();

    // --- RECORDING FRAME CAPTURE ---
    if (ffmpegPipe) {
      Image screen = LoadImageFromScreen();
      // Do not flip. LoadImageFromScreen returns an actionable image (Top-Left
      // origin). ImageFlipVertical(&screen);
      fwrite(screen.data, 1280 * 720 * 4, 1, ffmpegPipe);
      UnloadImage(screen);
    }
  }

  // Cleanup
  if (ffmpegPipe)
    _pclose(ffmpegPipe);

  // Cleanup
  for (auto &c : chunks)
    c.Unload();
  UnloadShader(concreteShader);
  UnloadAudioStream(voidHum);
  CloseAudioDevice();
  CloseWindow();

  return 0;
}
