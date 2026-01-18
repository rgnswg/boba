#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "pathfinding.h"
#include "entity.h"
#include "projectile.h"
#include "characters/mongo.h" // Incluimos a nuestro personaje

#define MAX_PATH_NODES 1024

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Boba - MOBA Modular");
    SetExitKey(0); 

    // --- Sistemas ---
    GameMap map;
    Map_Init(&map);

    ProjectileManager projMgr;
    Proj_Init(&projMgr);

    // --- Jugador (Mongo) ---
    Entity player;
    // Inicializamos al jugador como "Mongo", pasándole el gestor de proyectiles
    Mongo_Init(&player, &projMgr);
    player.position = (Vector3){ 0.0f, 1.0f, 0.0f };

    // --- Cámara ---
    Camera3D camera = { 0 };
    Vector3 cameraOffset = { 20.0f, 20.0f, 20.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 22.0f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    // --- Pathfinding & Input State ---
    Vector3 currentPath[MAX_PATH_NODES];
    int pathLength = 0;
    int currentPathIndex = 0;
    Vector3 targetMarker = player.position;
    float targetMarkerTimer = 0.0f;

    // --- UI State ---
    bool menuActive = false;
    Rectangle exitBtnBounds = { screenWidth/2.0f - 100, screenHeight/2.0f - 25, 200, 50 };
    bool shouldClose = false;

    // SetTargetFPS(60); // Comentado para liberar FPS

    while (!shouldClose && !WindowShouldClose())
    {
        float dt = GetFrameTime();

        // 1. Input Global
        if (IsKeyPressed(KEY_ESCAPE)) menuActive = !menuActive;

        if (menuActive) {
            // Lógica Menu
            if (CheckCollisionPointRec(GetMousePosition(), exitBtnBounds) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                shouldClose = true;
            }
        }
        else {
            // Lógica Juego

            // --- Input Habilidades ---
            // Q (Solo si tenemos la habilidad y no está en Cooldown)
            if (IsKeyPressed(KEY_Q)) {
                if (player.onQ && player.cdQ <= 0) {
                    // Lanzar Q hacia donde esté el mouse en el mundo
                    Ray ray = GetMouseRay(GetMousePosition(), camera);
                     if (ray.direction.y != 0) {
                        float t = -ray.position.y / ray.direction.y;
                        if (t >= 0) {
                            Vector3 mouseWorldPos = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                            player.onQ(&player, mouseWorldPos);
                            player.cdQ = player.maxCdQ; // Reset Cooldown
                        }
                     }
                }
            }

            // --- Input Movimiento (Click Derecho) ---
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_CONTROL)))
            {
                Ray ray = GetMouseRay(GetMousePosition(), camera);
                if (ray.direction.y != 0) {
                    float t = -ray.position.y / ray.direction.y;
                    if (t >= 0) {
                        Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                        targetMarker = hitPoint;
                        targetMarkerTimer = 2.0f;
                        // Pathfinding
                        pathLength = Path_Find(&map, player.position, hitPoint, currentPath, MAX_PATH_NODES);
                        currentPathIndex = 0;
                    }
                }
            }

            // --- Actualizar Movimiento Player ---
            if (pathLength > 0 && currentPathIndex < pathLength) 
            {
                Vector3 targetNodePos = currentPath[currentPathIndex];
                Vector3 diff = Vector3Subtract(targetNodePos, player.position);
                diff.y = 0;
                
                if (Vector3Length(diff) < 0.2f) {
                    currentPathIndex++;
                } else {
                    Vector3 dir = Vector3Normalize(diff);
                    player.position = Vector3Add(player.position, Vector3Scale(dir, player.speed * dt));
                }
            }

            // --- Actualizar Entidades y Sistemas ---
            Entity_Update(&player, dt);
            Proj_Update(&projMgr, dt);

            // --- Cámara Follow ---
            camera.target = player.position;
            camera.position = Vector3Add(player.position, cameraOffset);

            if (targetMarkerTimer > 0) targetMarkerTimer -= dt;
        }

        // --- Render ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                
                // Mapa
                Map_Draw(&map);

                // Debug Path
                if (pathLength > 0) {
                    if (currentPathIndex < pathLength) DrawLine3D(player.position, currentPath[currentPathIndex], WHITE);
                    for(int i = currentPathIndex; i < pathLength - 1; i++) {
                        DrawLine3D(currentPath[i], currentPath[i+1], WHITE);
                    }
                }

                // Target Marker
                if (targetMarkerTimer > 0) {
                    DrawCylinder(targetMarker, 0.5f, 0.5f, 0.2f, 10, Fade(GOLD, targetMarkerTimer > 0.5f ? 0.5f : targetMarkerTimer));
                }

                // Player (Polimórfico: Mongo sabe como dibujarse)
                if (player.onDraw) player.onDraw(&player);

                // Proyectiles
                Proj_Draw(&projMgr);

            EndMode3D();

            // UI Overlay
            DrawText(TextFormat("FPS: %d", GetFPS()), screenWidth - 100, 10, 20, BLACK);
            
            // UI Habilidades (Cooldowns)
            DrawText("Q", 20, screenHeight - 60, 30, (player.cdQ > 0) ? GRAY : BLACK);
            if (player.cdQ > 0) DrawText(TextFormat("%.1f", player.cdQ), 50, screenHeight - 60, 30, RED);

            if (menuActive) {
                // ... (Menu draw logic)
                 bool btnHover = CheckCollisionPointRec(GetMousePosition(), exitBtnBounds);
                DrawRectangleRec(exitBtnBounds, btnHover ? RED : MAROON);
                DrawRectangleLinesEx(exitBtnBounds, 2, WHITE);
                int textWidth = MeasureText("SALIR", 20);
                DrawText("SALIR", exitBtnBounds.x + (exitBtnBounds.width - textWidth)/2, exitBtnBounds.y + 15, 20, WHITE);
            }

        EndDrawing();
    }

    Map_Unload(&map);
    CloseWindow();
    return 0;
}
