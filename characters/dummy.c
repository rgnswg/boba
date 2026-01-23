#include "dummy.h"
#include "raymath.h"
#include <stdio.h>

void Dummy_OnDeath(Entity* self) {
    // "Inmortalidad": Resetear vida y reportar
    self->health = self->maxHealth;
    printf("Dummy: Auch! He resucitado. (HP restaurada a %.0f)\n", self->maxHealth);
}

void Dummy_Draw(Entity* self) {
    if (!self->active) return;

    // Color dinámico: Se pone rojo a medida que pierde vida
    Color bodyColor = ColorLerp(RED, BROWN, self->health / self->maxHealth);

    // Cuerpo: Cilindro fino
    DrawCylinder(self->position, 0.3f, 0.3f, 2.0f, 10, bodyColor);
    
    // Cabeza: Esfera azul
    Vector3 headPos = Vector3Add(self->position, (Vector3){0, 2.0f, 0});
    DrawSphere(headPos, 0.3f, BLUE);
}

void Dummy_Init(Entity* ent) {
    Entity_Init(ent);
    
    ent->maxHealth = 1000;
    ent->health = 1000;
    ent->radius = 0.5f; // Radio de colisión
    ent->team = TEAM_NEUTRAL; // Dummy es neutral (todos le pegan)

    ent->onDraw = Dummy_Draw;
    ent->onDeath = Dummy_OnDeath;
}

