#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "pathfinding.h"
#include "entity.h"
#include "projectile.h"
#include "characters/mongo.h"
#include "characters/dummy.h" 
#include <stddef.h> // Para NULL

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

    // --- Entidades ---
    Entity player;
    Mongo_Init(&player, &projMgr);
    player.position = (Vector3){ 0.0f, 1.0f, 0.0f };
    player.team = TEAM_BLUE; // Jugador es AZUL

    Entity dummy;
    Dummy_Init(&dummy);
    dummy.position = (Vector3){ 10.0f, 1.0f, 5.0f }; 
    // dummy ya se inicializa como NEUTRAL internamente

    Entity allyDummy;
    Dummy_Init(&allyDummy);
    allyDummy.position = (Vector3){ -5.0f, 1.0f, 0.0f }; // A la izquierda
    allyDummy.team = TEAM_BLUE; // Forzamos que sea aliado

    // Lista de blancos para los proyectiles
    Entity* targets[] = { &dummy, &allyDummy };
    int targetCount = 2;

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
        Entity* hoveredEntity = NULL; // Declarado al inicio del frame para ser visible en Draw

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

            // --- Lógica de Hover (Mouse Picking Global) ---
            Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
            
            // Iterar sobre todos los blancos posibles (targets)
            for (int i = 0; i < targetCount; i++) {
                Entity* ent = targets[i];
                if (!ent->active) continue;

                // Definir una Bounding Box aproximada para el humanoide
                // Centro en ent->position (pies). Ancho: radius*2, Alto: 2.5f
                Vector3 min = { ent->position.x - ent->radius, ent->position.y, ent->position.z - ent->radius };
                Vector3 max = { ent->position.x + ent->radius, ent->position.y + 2.5f, ent->position.z + ent->radius };
                BoundingBox box = { min, max };

                RayCollision boxHit = GetRayCollisionBox(mouseRay, box);
                if (boxHit.hit) {
                    hoveredEntity = ent;
                    break; // Asumimos que solo podemos hoveryear uno a la vez (el mas cercano o primero)
                }
            }

            // Cambiar Cursor y Lógica de Selección
            if (hoveredEntity) {
                // Es atacable si es Enemigo O es Neutral
                if (hoveredEntity->team != player.team || hoveredEntity->team == TEAM_NEUTRAL) {
                    SetMouseCursor(MOUSE_CURSOR_CROSSHAIR); 
                } else {
                    // Es Aliado (Curar/Buffear)
                    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); 
                }
            } else {
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            }

            // --- Input Habilidades ---
            if (IsKeyPressed(KEY_Q)) {
                if (player.onQ && player.cdQ <= 0) {
                    Vector3 targetPoint = {0};

                    if (hoveredEntity) {
                        targetPoint = hoveredEntity->position; // Auto-aim al centro de la entidad
                    } 
                    else 
                    {
                        // Fallback: Suelo
                        if (mouseRay.direction.y != 0) {
                            float t = -mouseRay.position.y / mouseRay.direction.y;
                            if (t >= 0) {
                                targetPoint = Vector3Add(mouseRay.position, Vector3Scale(mouseRay.direction, t));
                            }
                        }
                    }

                    // Disparar siempre (si hubo suelo o enemigo)
                    player.onQ(&player, targetPoint);
                    player.cdQ = player.maxCdQ; 
                }
            }

            // W (Dash hacia aliado)
            if (IsKeyPressed(KEY_W)) {
                if (player.onW && player.cdW <= 0) {
                    if (hoveredEntity && hoveredEntity->team == player.team) {
                        player.onW(&player, hoveredEntity->position);
                        player.cdW = player.maxCdW;
                        
                        // Cancelar pathfinding actual al dashear
                        pathLength = 0; 
                        currentPathIndex = 0;
                    }
                }
            }

            // --- Input Movimiento / Ataque Básico ---
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_CONTROL)))
            {
                if (!player.isDashing) { 
                    bool isAttackCommand = false;

                    // Si hacemos click en un enemigo -> Atacar
                    if (hoveredEntity && hoveredEntity->team != player.team && hoveredEntity->team != TEAM_NEUTRAL) {
                        player.targetEntity = hoveredEntity;
                        isAttackCommand = true;
                        // Feedback visual opcional: Flash rojo en el enemigo?
                    }
                    // O si es Neutral (Dummy) -> Atacar
                    else if (hoveredEntity && hoveredEntity->team == TEAM_NEUTRAL) {
                        player.targetEntity = hoveredEntity;
                        isAttackCommand = true;
                    }

                    if (isAttackCommand) {
                        // Cancelar pathfinding de movimiento al suelo
                        pathLength = 0;
                        currentPathIndex = 0;
                        targetMarkerTimer = 0; // Ocultar marcador de suelo
                    }
                    else {
                        // Movimiento normal al suelo
                        player.targetEntity = NULL; // Dejar de perseguir
                        
                        Ray ray = GetMouseRay(GetMousePosition(), camera);
                        if (ray.direction.y != 0) {
                            float t = -ray.position.y / ray.direction.y;
                            if (t >= 0) {
                                Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                                targetMarker = hitPoint;
                                targetMarkerTimer = 2.0f;
                                pathLength = Path_Find(&map, player.position, hitPoint, currentPath, MAX_PATH_NODES);
                                currentPathIndex = 0;
                            }
                        }
                    }
                }
            }

            // --- Actualizar Lógica de Ataque / Persecución ---
            if (player.targetEntity) {
                if (!player.targetEntity->active) {
                    player.targetEntity = NULL; // Murió
                } 
                else {
                    Vector3 diff = Vector3Subtract(player.targetEntity->position, player.position);
                    diff.y = 0;
                    float dist = Vector3Length(diff);

                    if (dist <= player.attackRange) {
                        // EN RANGO: Detenerse y Disparar
                        pathLength = 0; // Stop moving
                        
                        if (player.attackTimer <= 0 && player.onAttack) {
                            player.onAttack(&player, player.targetEntity->position);
                            player.attackTimer = player.attackCooldown;
                        }
                    } 
                    else {
                        // FUERA DE RANGO: Perseguir usando Pathfinding
                        // Recalculamos el camino hacia el enemigo dinámicamente
                        // Nota: Hacer A* cada frame es costoso en mapas grandes, pero OK aqui.
                        pathLength = Path_Find(&map, player.position, player.targetEntity->position, currentPath, MAX_PATH_NODES);
                        
                        if (pathLength > 0) {
                            // Moverse hacia el primer nodo del camino
                            Vector3 targetNodePos = currentPath[0];
                            Vector3 moveDiff = Vector3Subtract(targetNodePos, player.position);
                            moveDiff.y = 0;
                            
                            Vector3 dir = Vector3Normalize(moveDiff);
                            if (!player.isDashing) {
                                player.position = Vector3Add(player.position, Vector3Scale(dir, player.speed * dt));
                            }
                        }
                    }
                }
            }
            // --- Actualizar Movimiento Player (Solo si NO estamos persiguiendo/atacando a alguien) ---
            else if (!player.isDashing && pathLength > 0 && currentPathIndex < pathLength) 
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
            Entity_Update(&dummy, dt); 
            Entity_Update(&allyDummy, dt);
            
            // Proyectiles chequean colisiones contra targets
            Proj_Update(&projMgr, dt, targets, targetCount);

            // --- Cámara Follow ---
            camera.target = player.position;
            camera.position = Vector3Add(player.position, cameraOffset);

            if (targetMarkerTimer > 0) targetMarkerTimer -= dt;
        }

        // --- Render ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                
                Map_Draw(&map);

                if (pathLength > 0) {
                    if (currentPathIndex < pathLength) DrawLine3D(player.position, currentPath[currentPathIndex], WHITE);
                    for(int i = currentPathIndex; i < pathLength - 1; i++) {
                        DrawLine3D(currentPath[i], currentPath[i+1], WHITE);
                    }
                }

                if (targetMarkerTimer > 0) {
                    DrawCylinder(targetMarker, 0.5f, 0.5f, 0.2f, 10, Fade(GOLD, targetMarkerTimer > 0.5f ? 0.5f : targetMarkerTimer));
                }

                if (player.onDraw) player.onDraw(&player);
                if (dummy.onDraw) dummy.onDraw(&dummy);
                if (allyDummy.onDraw) allyDummy.onDraw(&allyDummy);

                // Highlight de Enemigo/Aliado
                if (hoveredEntity) {
                    Color ringColor = YELLOW; // Neutral
                    if (hoveredEntity->team == player.team) ringColor = GREEN; // Aliado
                    else if (hoveredEntity->team != TEAM_NEUTRAL) ringColor = RED; // Enemigo

                    DrawCylinderWires(hoveredEntity->position, hoveredEntity->radius * 1.5f, hoveredEntity->radius * 1.5f, 0.1f, 16, ringColor);
                }

                Proj_Draw(&projMgr);

            EndMode3D();

            // UI Overlay
            DrawText(TextFormat("FPS: %d", GetFPS()), screenWidth - 100, 10, 20, BLACK);
            
            // UI Habilidades
            DrawText("Q", 20, screenHeight - 60, 30, (player.cdQ > 0) ? GRAY : BLACK);
            if (player.cdQ > 0) DrawText(TextFormat("%.1f", player.cdQ), 50, screenHeight - 60, 30, RED);

            DrawText("W", 100, screenHeight - 60, 30, (player.cdW > 0) ? GRAY : BLACK);
            if (player.cdW > 0) DrawText(TextFormat("%.1f", player.cdW), 130, screenHeight - 60, 30, RED);

            // UI Dummy HP (Truco para proyectar coordenadas 3D a 2D)
            Vector3 headPos = Vector3Add(dummy.position, (Vector3){0, 2.5f, 0});
            Vector2 screenPos = GetWorldToScreen(headPos, camera);
            DrawText(TextFormat("HP: %.0f/%.0f", dummy.health, dummy.maxHealth), 
                     (int)screenPos.x - 40, (int)screenPos.y, 20, BLUE);

            if (menuActive) {
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