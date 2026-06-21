# Lista de tareas — port 1:1 de Minecraft a C++

> Lista de tareas proporcionada por el usuario. Es la referencia de trabajo que
> los agentes (Codex / Claude) van completando. **Regla #0:** no se tantean ni se
> inventan valores — todo se porta del source decompilado (`26.1.2/src/`) y de los
> datos de worldgen (`26.1.2/data/`). Ver `AGENTS.md` y `CLAUDE.md`.
>
> Las casillas se dejan sin marcar salvo confirmación explícita de que la tarea
> está terminada y verificada.

## Vegetación / decoración

- [ ] Los troncos caídos pueden spawnear encima de árboles.
- [ ] Los troncos no tienen la característica de tener una dirección de placement, entonces los troncos caídos tienen todos sus segmentos mirando hacia arriba en vez de hacia al lado.
- [ ] Hay un tipo de hierbajo en concreto que spawnea con una frecuencia anormal y es gris porque no tiene la lógica de coloreado como otros bloques (por ejemplo la hierba).
- [ ] Los troncos caídos a veces spawnean con una longitud anormal y tienen una parte flotando en el aire.
- [ ] Las flores se renderizan como hierba, con dos planos que se interseccionan, cuando no son así realmente: son un plano tumbado en el suelo ligeramente levantado. Exactamente el mismo comportamiento con las enredaderas (esta vez son un plano pegado a un bloque en vez de al suelo) y lo mismo con las vines que brillan en las cuevas.
- [ ] Las algas parecen estar implementadas pero no se ven spawnear en el mar; solo ocasionalmente spawnean buggeadas encima de un bloque de hierba encima del mar.
- [ ] A veces se generan conjuntos de hongos en el suelo en zonas en las que no deberían (no estoy seguro de que esto sea un bug, puede ser comportamiento normal).
- [ ] A veces spawnean árboles encima de glaciares en medio del mar.
- [ ] La textura del bambú es incorrecta: el bambú es como una especie de modelo 3D con texturas que lo rodean y necesita una implementación con especial cariño.
- [ ] La hierba en el juego original spawnea con un displacement específico generado en base a las coordenadas en las que está y una función de ruido; lo mismo pasa para el bambú y otras cosas (aplica a todos los tipos de hierba).
- [x] Los árboles generados cerca del borde de un chunk tienen las hojas/tronco que se desbordan al chunk vecino cortadas, en vez de escribirse en el vecino. (Regresión reintroducida al tomar la versión rama del BiomeDecorator en el merge del PR#6; arreglado re-cableando `chunkAt` desde el caller → `ChunkWGL` → `TreeWorld`, como en el commit 9307b4c.)

## Rendimiento / race conditions

- [ ] Hay race conditions que hacen que el chunk que se genere esté corrupto. Solo pasa al ir rápido, y a peor CPU más pasa.
- [ ] Hay que compilar todo a un solo ejecutable. Si el ejecutable se saca de la carpeta funciona, pero no se generan árboles ni nada; supongo que hay otros ejecutables en la carpeta de `/build` de los que depende `mcpp.exe`. Compila todo a uno. Además, seguramente el tener un proceso de generación de decoración esté quitando rendimiento y causando los problemas de chunks corruptos.
- [ ] La generación de terreno presenta problemas de rendimiento, debe ser optimizada; hace que el juego tenga stutters.
  - [ ] Perfil 2026-06-20 22:40 UTC: `terrain_engine_perf --radius 4 --seed 1` bajó de `fillFromNoise` 190.5 ms/chunk + `buildSurface` 105.8 ms/chunk a 138.3 + 76.4 ms/chunk tras cachear generadores/RandomState/SurfaceSystem, usar setter de fase NOISE y cachear modelos vanilla por `stateId`. Sigue abierta: density functions/surface rules son aún demasiado lentos.
  - [ ] Mitigación 2026-06-21 17:56 UTC: el freeze de streaming no venía solo de generación async; decoración (`applyBiomeDecoration` + estructuras) y meshing corrían en el hilo principal. `--quickPlaySingleplayer` usa `startLocalGameFast`, `startLocalGame` ya no decora el spawn chunk síncronamente, la integración se limita a 2 chunks/tick, la decoración se pausa mientras hay movimiento/input reciente (750 ms) y el meshing queda a 1 rebuild/frame. Sigue abierta la solución final: decoración/meshing con snapshot worker o contexto thread-safe real.
  - [ ] Mitigación 2026-06-21 18:04 UTC: sigue injugable a ~2 FPS mientras genera; se redujo el pool de streaming a 1 worker en CPUs típicas (2 en CPUs grandes), los workers entran en background mode en Windows, y el renderer difiere rebuild/upload de meshes si hubo input/cámara en los últimos 250 ms. Prioriza que el jugador pueda moverse aunque los chunks visuales aparezcan tarde. Pendiente: meshing/decoración en workers con snapshots.
  - [ ] Arquitectura 2026-06-21 18:15 UTC: se retiró el diferido de meshing/upload por movimiento porque paraba el catch-up visual. `ChunkMesher::buildChunk` ahora corre en un worker dedicado sobre snapshots privados de `LevelChunk` (centro + vecinos cardinales), el render thread solo integra futures listas y las subidas de secciones GPU quedan presupuestadas a 2/frame. Pendiente: aplicar la misma idea a decoración, que sigue siendo el hot path síncrono restante.
  - [ ] Hotfix 2026-06-21 18:22 UTC: corregido el assert de MSVC `can't dereference value-initialized vector iterator` tras mover meshing a worker. La cola de builds pendientes ya no borra por iterador y las caches de modelos del mesher devuelven copias estables en vez de referencias/punteros a storage mutable.
- [ ] Cuando los chunks se están generando y el usuario va muy rápido e intenta mover la cámara, la cámara da como un lagback que se siente terrible y no debería existir.

## Menú / UI

- [ ] El menú principal tiene un montón de cosas que faltan, problemas de funcionamiento y cosas que no son como en el juego original:
  - [x] Hacer click en Singleplayer crasheaba tras los cambios de streaming. Arreglado evitando que callbacks de botones destruyan la pantalla/vector de widgets mientras `mouseClicked` sigue ejecutándose: las pantallas copian la acción seleccionada y la invocan después de dejar de tocar sus botones.
  - [x] Los sliders solo reaccionan al click, no a la acción de deslizar el mouse con el click presionado. (Arreglado portando la semántica de `AbstractSliderButton`: click inicia drag, `mouseDragged` actualiza con `(mouseX - (x + 4)) / (width - 8)`, `mouseReleased` suelta el handle; eventos propagados desde Win32 -> Screen -> widget.)
  - [ ] El indicador de dónde está el slider (que es como un rectángulo) no tiene texturas o parece un placeholder.
  - [ ] Faltan muchas opciones dentro de todos los submenús.
  - [ ] Dentro de Video, la opción de Gráficos tiene un corte en el botón, como si la textura se repitiera por un UV mal puesto o similar.
  - [x] Hay que añadir funcionalidad al botón de GUI Scale. (Arreglado aplicando la escala GUI con el algoritmo Java `Window.calculateScale`: Auto respeta el mínimo 320x240, las pantallas se reinicializan con tamaño lógico escalado, el mouse se convierte a coordenadas GUI y el render 2D usa dimensiones lógicas.)
  - [ ] Hay que terminar de añadir todos los botones y opciones dentro de los submenús.
  - [ ] Hay que añadir la interfaz del selector de mundos singleplayer para hacer un port 1:1 de verdad.
  - [x] Hay que añadir el menú de pausa (ESC) ingame para poder volver al menú, etc. (Añadido `PauseScreen`: ESC abre/cierra pausa en mundo, Back to Game reanuda y recaptura mouse, Disconnect vuelve al TitleScreen; Options desde pausa vuelve a pausa al pulsar Done. También se corrigió que quickplay reabriera el TitleScreen al inicializar texturas GUI.)

  - [ ] Perfil 2026-06-21 00:10 UTC: causa base encontrada en el router/caches Java. C++ omitia `flatCache(cache2d(...))` de `NoiseRouterData` y el `CornerResolver` anulaba marcadores `cache2d`/`flatCache`; ademas `buildSurface` recacheaba biomas y recalculaba heightmaps completos en vez de mantenerlos como Java. Nuevo `terrain_engine_perf --radius 4 --seed 1`: `fillFromNoise` 77.2 ms/chunk, `buildSurface` 18.8 ms/chunk, `applyCarvers` 4.1 ms/chunk, `chunkMesh` 62.1 ms/chunk. Sigue abierta: `fillFromNoise` y `chunkMesh` aun son demasiado altos.

## Menas / minerales

- [ ] La generación de menas en la mayor parte funciona bien, pero a veces spawnean conjuntos gigantes de menas como diamantes que nunca se verían en el juego normal en estas cantidades.
- [ ] Las menas suelen generar en la superficie de las montañas minerales totalmente anormales, especialmente esmeraldas. Todos en general en una cantidad anormal; diría que no está implementada la lógica de distribución de menas según capa correctamente.

## Estructuras

- [ ] Los portales arruinados no tienen la lógica de generación del juego original, o a veces spawnean en medio del mar totalmente (es posible que este comportamiento sea original, no estoy totalmente seguro de marcar esto como un bug).
- [ ] La estructura de los portales arruinados no coincide con la original: la original tiene bloques de lava, magma... este no lo tiene. La textura de un bloque al lado del portal arruinado (imagino que del cofre que siempre spawnea al lado) está completamente rota y deja ver una mayor parte del atlas de texturas.
- [ ] Hay que acabar completamente el plan de implementación de estructuras.

## Texturas / coloreado

- [ ] Hay que implementar las texturas animadas del agua, la lava, las algas...
- [ ] La sabana no tiene el color característico de la hierba: Minecraft guarda las texturas como la de la hierba en grisáceo y se colorean programáticamente después; esto no está implementado y se ve visualmente horrible en biomas como la sabana.

## Biomas / terreno

- [ ] Faltan muchos biomas por implementar; la variedad es muy limitada y la dispersión es anormal en comparación con el juego original.
- [ ] Probablemente falten terrain features de montañas... es bastante plano el terreno en general.
- [ ] Parece que hay un intento de generación de bioma de pantano pero está fatal implementado: hay nenúfares que spawnean encima de la tierra, los nenúfares se ven como un bloque gris y no están coloreados programáticamente (siguen la misma lógica que las flores, un plano en un bloque ligeramente levantado), la generación de terreno no es una generación de terreno de pantano, hay lagos, es todo más plano, la hierba cambia de color... simplemente está implementado de manera muy pobre.
- [ ] Implementa correctamente todos los biomas; debería haber sobre 65 biomas en el juego original. Ten en cuenta los bloques que spawnean de manera natural en cada bioma e impleméntalos.

## Revisión final

- [ ] Finalmente haz una revisión general de qué tan 1:1 es el generador. Yo sé en el fondo que probablemente sea una aproximación chapucera, pero el objetivo final sería tener un generador que, dada la misma semilla que en el juego original, diera el mismo mundo. No te pido que lo arregles, solamente que lo valores.

## Proceso

- [ ] Cada tarea sube el progreso a git.
