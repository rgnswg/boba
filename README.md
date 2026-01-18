# BOBA, el moba del pueblo

Este es un proyecto recreativo, la idea es aprovechar la sabiduría de los LLM's para armar un juego complejo usando raylib y C puro, evitando el overhead de los motores graficos.

## Explicación mini

### `main.c`
Inicializa todos los sistemas (mapa, proyectiles), gestiona el bucle principal (input, update, render) y la UI base (menú, FPS).
### Módulos Principales
 *   **`map.c` / `map.h`:** Gestiona la carga del nivel desde `map.png`. Construye un único modelo 3D estático para todas las paredes y provee la lógica de transitabilidad.
*   **`pathfinding.c` / `pathfinding.h`:** Contiene el algoritmo A* que calcula las rutas óptimas para el personaje, respetando los obstáculos del mapa.
*   **`entity.c` / `entity.h`:** Define la estructura base `Entity` para todos los personajes, incluyendo atributos y punteros a funciones para habilidades y comportamiento específicos.
*   **`projectile.c` / `projectile.h`:** Implementa un gestor simple de proyectiles que controla su movimiento y ciclo de vida en el juego.
### Mapas
Los mapas se definen mediante el archivo `map.png`. Los píxeles claros (`>100 de R`) son terreno transitable, los oscuros son obstáculos. Al inicio, un único modelo 3D del mapa es construido y optimizado para alto rendimiento.
### Personajes
Se crean usando el sistema `Entity`. Cada personaje (ej. `characters/mongo.c`) implementa sus habilidades y aspecto visual conectándolos a los punteros a función de su `Entity` base.
