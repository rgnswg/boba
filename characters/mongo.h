#ifndef MONGO_H
#define MONGO_H

#include "../entity.h"

// Inicializa a Mongo. Requiere el contexto (ProjectileManager)
void Mongo_Init(Entity* ent, void* projectileManager);

#endif
