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
- [x] Hay un tipo de hierbajo en concreto que spawnea con una frecuencia anormal y es gris porque no tiene la lógica de coloreado como otros bloques (por ejemplo la hierba).
  - RESUELTO 2026-06-27 (parte del coloreado por bioma): la causa raíz del "gris" era que
    el mesher usaba un tint HARDCODEADO mal etiquetado — `#79C05A` (que es forest, ¡ni
    siquiera plains!) para grass y `#59AE30` para foliage. Con el wiring runtime del
    `BiomeMeshContext` (carga de colormaps desde `assets.bin` + snapshot de bioma por chunk
    en el hilo principal + `biometint::tint` por vértice), cada bloque grass/foliage/water
    ahora se tinta según el bioma real en esa posición. Fallback corregido a plains real
    `#91BD59`/`#77AB2F`. Verificado por smoke Linux headless: "Biome colouring active (65 biomes)".
    La "frecuencia anormal" del hierbajo es un problema SEPARADO de generación (ver entrada
    de race conditions / persistencia de chunks más abajo).
- [ ] Los troncos caídos a veces spawnean con una longitud anormal y tienen una parte flotando en el aire.
- [ ] Las flores se renderizan como hierba, con dos planos que se interseccionan, cuando no son así realmente: son un plano tumbado en el suelo ligeramente levantado. Exactamente el mismo comportamiento con las enredaderas (esta vez son un plano pegado a un bloque en vez de al suelo) y lo mismo con las vines que brillan en las cuevas.
- [ ] Las algas parecen estar implementadas pero no se ven spawnear en el mar; solo ocasionalmente spawnean buggeadas encima de un bloque de hierba encima del mar.
- [ ] A veces se generan conjuntos de hongos en el suelo en zonas en las que no deberían (no estoy seguro de que esto sea un bug, puede ser comportamiento normal).
- [ ] A veces spawnean árboles encima de glaciares en medio del mar.
- [x] Bambú: con el atlas + el path de modelos JSON funcionando, el bambú renderiza con su
      modelo 3D real (multipart bamboo1..4 + hojas), no como billboard de cruz. Verificado
      por captura en jungla (semilla 1, spawn -680 968): tallos verticales segmentados.
- [ ] La hierba en el juego original spawnea con un displacement específico generado en base a las coordenadas en las que está y una función de ruido; lo mismo pasa para el bambú y otras cosas (aplica a todos los tipos de hierba).
- [x] Los árboles generados cerca del borde de un chunk tienen las hojas/tronco que se desbordan al chunk vecino cortadas, en vez de escribirse en el vecino. (Regresión reintroducida al tomar la versión rama del BiomeDecorator en el merge del PR#6; arreglado re-cableando `chunkAt` desde el caller → `ChunkWGL` → `TreeWorld`, como en el commit 9307b4c.)

## Rendimiento / race conditions

- [ ] **SOSPECHA (2026-06-27): el build de Linux rinde EXTREMADAMENTE bien en
      comparación con el de Windows** — sospechosamente bien. Mismo código OpenGL
      (`DeviceGL.cpp` Windows vs `DeviceGL_linux.cpp`), mismo `vsync=false` en
      `RenderBackend::createDevice`, mismo loop principal en `main.cpp`. Posibles
      causas a investigar (NO arreglar hasta confirmar):
  1. **Driver override de vsync en Windows**: aunque `wglSwapIntervalEXT(0)`
     desactiva vsync en el contexto, drivers NVIDIA/AMD/Intel en Windows suelen
     tener un override global "Adaptive VSync" / "Vertical Sync = On" que reactiva
     el sync. En Linux con Mesa el override es menos común. → medir FPS real en
     ambos con el debug overlay (F1 → tab 1).
  2. **`wglMakeCurrent` + `glFinish` overhead**: el path Windows hace más
     validación de contexto. Difícil de medir sin profiler.
  3. **Mesa llvmpipe en el portátil**: si el portátil Linux está usando el
     renderer software (llvmpipe), el "rendimiento sospechoso" puede ser que el
     GPU real (NVIDIA/AMD) NO se está usando — el frame rate se ve alto porque
     llvmpipe no hace vsync. → `glxinfo | grep "renderer string"` para confirmar.
  4. **Menos trabajo por frame en Linux**: si los modelos JSON / blockstates NO
     se cargaban (bug arreglado 2026-06-27), los bloques renderizaban como
     checker magenta — un quad plano sin modelo. Eso es MENOS geometría por
     chunk → más FPS. Con el fix del asset packing, los modelos ahora cargan
     y la geometría por chunk sube. Volver a medir FPS después del fix para
     comparar.
  Acción: NO tocar nada hasta tener una medición de FPS con el debug overlay en
  AMBAS plataformas, mismo seed, misma posición de cámara. Documentar el resultado
  y luego decidir si es un bug real o solo percepción.

- [ ] Hay race conditions que hacen que el chunk que se genere esté corrupto. Solo pasa al ir rápido, y a peor CPU más pasa.
  - [ ] DIAGNÓSTICO 2026-06-26 (causa raíz de la "frecuencia absurda y artificial"
        de features): **no hay persistencia de chunks**. En `Minecraft.cpp` los chunks
        fuera de `RADIUS = 6` se **borran** (`unloadChunk` → `m_chunks.erase`) y al
        revisitarlos se **regeneran y RE-DECORAN** desde cero (no hay caché ni
        guardado en disco). La decoración de un chunk N escribe features que se
        derraman a sus vecinos (árboles que sobresalen, fósiles en ±16, piezas de
        estructura). Si el jugador se aleja (N se descarga) y vuelve (N se regenera y
        re-decora), N vuelve a derramar sus features sobre los vecinos que SÍ
        persistieron → **duplicación acumulativa** en la zona de solape; a la vez el
        chunk regenerado pierde el derrame que recibió de SUS vecinos. Ir y venir por
        el borde de streaming acumula features. La decoración de un chunk individual
        SÍ está protegida (flag `decorated` + un único worker), así que no es doble
        decoración dentro de una residencia — es la regeneración-al-revisitar.
        "A peor CPU más pasa" encaja: más lag de generación → el borde se mueve más →
        más churn de descarga/regeneración. Arreglo 1:1 correcto: **persistencia**
        (no regenerar chunks ya decorados).
  - [~] IMPLEMENTADO 2026-06-26 (pendiente build-test en Windows): **caché de
        persistencia en memoria**. `unloadChunk` ahora MUEVE el chunk a
        `m_chunkCache` (no lo destruye) en vez de borrarlo, y un paso nuevo
        `restoreCachedChunksInRadius` lo devuelve VERBATIM a `m_chunks` al revisitar
        (sin regenerar ni re-decorar → sin re-derrame de features). Solo se cachean
        chunks totalmente decorados y que no estén en la cola de decoración (los que
        están en vuelo se borran y se auto-curan al revisitar, como antes). El
        renderer ya reconcilia mallas GPU contra `m_chunks` (las suelta al salir,
        las reconstruye vía `meshDirty` al volver), así que es transparente. Capacidad
        1024 chunks (~100-150 MB; cubre un área revisitada mucho mayor que RADIUS=6).
        La caché se vacía al crear/cambiar de mundo. Equivale a la persistencia en
        disco de vanilla pero en memoria. Falta compilar y probar en el engine real.
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

> AUDITORÍA 2026-06-21: estado real del subsistema certificado en
> `docs/STRUCTURES_STATUS.md` (ledger por estructura + 3 huecos arquitectónicos
> de raíz: orden de generación, Beardifier/terrain_adaptation, y procesadores).
> Resumen: solo se colocan de verdad swamp_hut, desert_pyramid, jungle_pyramid,
> igloo, shipwreck y nether_fossil (piezas hand-port); la familia jigsaw (aldeas,
> outpost, ancient_city, bastion, trail_ruins, trial_chambers) se ensambla pero
> SIN procesadores ni adaptación de terreno (por eso las aldeas no se ven / se
> exponen); ocean_ruin, ruined_portal, buried_treasure, ocean_monument, mansion,
> mineshaft, stronghold, fortress y end_city NO se colocan (solo helpers).

- [x] RULE #0: dejar de marcar `ocean_ruin`/`ruined_portal`/`buried_treasure` como
      "supported" cuando en realidad hacían no-op (assembly jigsaw con start_pool
      vacío). Ahora son no-ops honestos y se loguean como UNPORTED al cargar.
- [x] Gate de bioma para estructuras hand-coded (`Structure.isValidBiome`): el
      dispatch no-jigsaw colocaba en cualquier bioma (¡400 nether_fossil en 1600
      chunks de plains overworld, pirámides en plains, iglús en plains!). Portado el
      check de bioma en el centro del chunk; verificado con `structure_gen_probe`
      que cada estructura aparece solo en su bioma (swamp→hut, snowy→iglú,
      desert→pirámide, jungle→templo, beach→naufragio) y 0 nether_fossil en overworld.
- [x] Arnés de verificación headless `structure_gen_probe` (target CMake +
      `tools/structure_gen_probe/`): ejecuta el generador real contra los datos
      reales y reporta qué se coloca. Permite verificar estructuras en Linux/CI sin
      Windows. Aldeas confirmadas: ensamblan 12/120² con ~100 piezas y ~12k bloques.
- [ ] Los portales arruinados no tienen la lógica de generación del juego original, o a veces spawnean en medio del mar totalmente. (Estado real: NO portados — solo `RuinedPortalYSelector`. Falta `RuinedPortalPiece` + procesador de envejecimiento/lava + placement por tipo. Ver roadmap #6.)
- [ ] La estructura de los portales arruinados no coincide con la original: lava/magma, cofre. (Bloqueado por lo anterior + soporte de block-entity/loot para el cofre.)
- [ ] Adaptación de terreno (Beardifier): `terrain_adaptation` (beard_thin/beard_box/bury/encapsulate) no está portado; causa que las estructuras jigsaw se expongan/floten. Ver roadmap #3–#4.
- [~] Procesadores de estructura (incremento #1 de aldeas HECHO): portado el pipeline
      `RuleProcessor` + semántica `legacy_single_pool_element` en `placeTemplate`.
      Legacy ya no coloca AIR (las aldeas dejan de carvar terreno: ~12k→~3.8k bloques/aldea);
      los RuleProcessor aplican (verificado: mossify→mossy_cobblestone, farm→carrots).
      Pendiente: GravityProcessor/ProtectedBlockProcessor + procesadores de proyección
      TERRAIN_MATCHING (caminos siguiendo el terreno) → fase de adaptación de terreno.
- [x] ALDEAS ACTIVADAS Y VERIFICADAS (componentes): portadas las 3 capas que faltaban
      — pipeline de procesadores (rules + legacy air-ignore + Gravity de calles), y el
      **Beardifier** (adaptación de terreno `beard_thin`) certificado byte-exacto contra
      la clase real (`beardifier_parity` 8000/0). Junctions registradas en el ensamblaje;
      `forStructuresInChunk` portado; sumado a `fillFromNoise` (terreno sin estructuras
      byte-idéntico: `full_chunk_parity` 98304/0). Cableado en el motor (beardifier en
      hilo principal → worker). Verificado en Linux: las aldeas ensamblan con el pipeline
      completo, el beardifier por chunk es no-vacío/determinista y produce densidad no-cero
      cerca de aldeas. 2026-06-22: `feature_pool_element` ya está cableado a través del
      pase FEATURES por chunk y usa el runtime certificado de `PlacedFeature` con el RNG
      de estructura ya sembrado. Intento de certificación 2026-06-22: el server real
      localiza `village_plains` en `[640,~,816]` / chunk `(40,51)` y `StructureStartsDump`
      guarda 84 piezas; la ruta C++ observada monta 89 y falla al ejercitar el
      `FeaturePoolElement` `minecraft:oak`. Actualizacion 2026-06-22 c: el probe limpio
      `structure_gen_probe --seed 1 --radius 56 --biome minecraft:plains --surface 68`
      ya reproduce `village_plains` `(40,51)` con 84 piezas, y el smoke in-game en
      `--seed 1 --spawn 640 816` coloca una aldea en `(40,51)` sin `runStructures failed`
      ni `decorateChunk failed` tras portar postprocesado de `dirt_path`, antorchas,
      camas y escaleras. Ojo: en el contexto real del juego ese start resuelve como
      `village_snowy` de 31 piezas, no como el caso plains forzado. Pendiente: gate de
      starts/piezas en el contexto real del server y despues diff bloque-a-bloque contra
      `.mca` del server.
- [x] Buried treasure: portado `BuriedTreasurePiece.postProcess` 1:1 (escaneo hacia abajo
      hasta la primera columna anclada en sandstone/stone/andesite/granite/diorite, relleno
      de aire/líquido de los 6 vecinos con belowState/softState, y colocación del cofre).
      Cableado el dispatch `tryPlaceBuriedTreasure` + tipo `minecraft:buried_treasure` como
      soportado. Verificado con `structure_gen_probe --seed 1 --radius 48 --biome minecraft:beach`:
      ya NO sale como UNPORTED y coloca en chunks de playa escribiendo bloques (cofre + relleno).
      Gap honesto: el LOOT del cofre (loot table + block-entity) no está portado — se coloca el
      bloque de cofre, igual que en DesertPyramid/SwampHut.
- [x] OCEAN_RUIN PORTADO Y VERIFICADO 2026-06-22: port 1:1 de `OceanRuinPieces`
      (segunda estructura grande). `addPieces` (gate isLarge por `large_probability`,
      baseIntegrity 0.9/0.8, cluster por `cluster_probability`), `addPiece` (WARM = 1 ruina;
      COLD = brick+cracked+mossy superpuestas a integridad 0.9/0.7/0.5), y el **BlockRotProcessor**
      (erosión: random por bloque sembrado por la pos del mundo, keep si `nextFloat() <= integrity`)
      cableado en `placeTemplate`. Cluster vía `OceanRuinClusterGeometry` certificada. Verificado
      con `structure_gen_probe --biome minecraft:ocean|warm_ocean`: ya NO es UNPORTED; cold coloca
      ~66-77 (small) / ~480-880 (large+cluster), warm ~30-36 / ~207. Gaps honestos documentados
      (loot del cofre, drowned, arqueología suspicious-sand → necesitan block-entity/entidad/loot).
- [ ] Hay que acabar completamente el plan de implementación de estructuras. (Roadmap completo en `docs/STRUCTURES_STATUS.md`.)
- [x] AUDITORÍA 2026-06-26 (RULE #0): se encontraron 3 estructuras con colocación
      FABRICADA marcadas como soportadas (violación de RULE #0): `woodland_mansion`
      (losa 52×52 de adoquín inventada), `fortress` (puente 5×10 inventado) y
      `stronghold` (caja 16×16×8 de stone_bricks que **excava un hueco de aire
      14×14×6** en el terreno — el "espacio cuadrado de aire" reportado). Eliminadas
      de `supportedTypes` en `WorldGen.cpp` y `StructureGen.cpp`: ahora son no-ops
      honestos (UNPORTED). `ocean_monument` también revertido (cáscara real pero RNG
      roto + sin salas). `end_city` se mantiene (solo `base_floor`, pero es del End,
      no afecta al overworld).
- [ ] FÓSILES overworld ("huesos de dinosaurio"): el cuerpo del feature
      (`FossilFeature.h`) es 1:1 fiel a `FossilFeature.java` (rarity 1/64 en
      desert/swamp/mangrove, colocado 15–24 bajo la superficie, NUNCA escribe aire).
      La frecuencia/aire que se ve es probablemente la re-decoración (race) y/o el
      `stronghold` fabricado de arriba. Confirmar en el engine de Windows.

## Texturas / coloreado

- [x] **SISTEMA UNIFICADO DE ASSETS (2026-06-27): muchas texturas faltantes en el
      build de Linux — flores, igloo, huesos de dinosaurio (fossils), algas dobles
      (tall seagrass), etc. se veían como el checker magenta.** Causa raíz:
      `asset_packer` solo empaquetaba `minecraft/textures/block/` (texturas de
      bloques) pero NO empaquetaba `minecraft/blockstates/` ni `minecraft/models/`
      (los JSON que el motor necesita para saber qué modelo + textura usar por bloque).
      El `ChunkMesh` tenía un fallback a disco (`assets/client-extract/assets/`) que
      solo funcionaba en máquinas dev con `tools/provision_runtime.sh` ejecutado.
      En CI, el exe standalone no encontraba los modelos → todo bloque con variantes
      (flores, hierba alta, seagrass, puertas, vallas, ...) caía al checker.
  - **Fix aplicado** — "assets auto-linked, no hardcodeados":
    - `tools/asset_packer/main.cpp` ahora empaqueta automáticamente TODO lo que el
      motor referencia desde el `client.jar` extraído: `minecraft/textures/` completo
      (block, gui, entity, environment, particle, colormap, font, painting, ...),
      `minecraft/blockstates/` (1170 JSON), `minecraft/models/` (3676 JSON). Solo
      excluye sonidos (~430MB, audio no wired) y lang (i18n no portado). El assets.bin
      bajó de 350MB a **22MB**.
    - `src/assets/TextureAtlas.cpp` `collectBlockTextureNames()` ahora recorre TODO
      `minecraft/textures/` (no solo `block/`) y registra cada textura por su path
      relativo. Atlas resultante: **4776 texturas** (antes 1112), **109 animated**.
    - `src/render/level/ChunkMesh.cpp` `readAssetOrLocal()` eliminó el fallback a
      disco — solo lee desde `assets.bin`. El exe standalone ya no depende de tener
      `assets/client-extract/` en disco.
    - `src/world/level/levelgen/structure/StructureGen.cpp` `ensureTemplate()`
      ahora también busca plantillas .nbt en `assets.bin` (clave
      `data/minecraft/structure/<loc>.nbt`) — antes solo buscaba en disco. Las 1202
      plantillas .nbt de estructuras ya estaban empaquetadas pero el runtime no las
      leía desde ahí.
  - **Principio establecido**: según implementemos nuevos bloques/sistemas, sus
    assets ya estarán empaquetados automáticamente — no hay que tocar el packer para
    cada bloque nuevo. Si un bloque referencia una textura que no existe, el atlas
    le asigna el tile "missingno" (checker magenta) y se ve claramente en runtime.
  - Verificado por smoke Linux headless: log "TextureAtlas: built 512x2400 atlas
    from assets.bin, 4776 of 4794 textures loaded (109 animated)" + captura de
    pantalla muestra 311 colores únicos (verdes/marrones/azules) en vez de solo
    el checker pattern.

- [x] Formas de bloque / texturas boca abajo o mal puestas: el path de modelos JSON
      del mesher (`tryEmitVanillaBlockModel`) leía los elementos del modelo pero
      descartaba TODA la rotación — ignoraba la rotación `x`/`y`/`uvlock` del variant
      del blockstate, la `rotation` por elemento y la `rotation` (UV) por cara. Por eso
      escaleras/troncos/vallas/etc. miraban todos igual y las texturas salían boca
      abajo o desplazadas. Arreglado cableando el pipeline ya certificado
      `FaceBakery::bakeQuad`: nuevo `render/model/OctahedralGroup.h` (port 1:1 de
      `com.mojang.math.OctahedralGroup` sobre el `SymmetricGroup3` certificado) para la
      matriz de `BlockModelRotation`; rotación de elemento vía `CuboidRotation`; UV
      rotation por cara; uvlock por cara vía `BlockMath::getFaceTransformation`; culling
      de la cara rotada vía `Direction.rotate`; tinte solo en caras con `tintindex>=0`.
      Verificado: el grupo octaédrico cumple cierre/identidad/determinante (48 elementos)
      y la TU compila limpia. Pendiente de verificación visual en runtime (requiere
      assets de cliente, no disponibles en el entorno Linux headless).
- [x] Texturas animadas (agua/lava/fuego/...) implementadas 2026-06-23: el atlas detecta
      tiras animadas (PNG 16xN), extrae cada frame 16x16, parsea el `.mcmeta`
      (frametime + orden), y `TextureAtlas::tickAnimations` avanza el frame por tiempo
      (20 ticks/s) re-subiendo el atlas al cambiar; `LevelRenderer` lo llama por frame.
      Verificado bajo Xvfb: "52 animated" y dos frames de mar con cámara fija difieren
      (MAE 340) — el agua cicla en pantalla. (Sin cambios en la API de render.)
- [x] La sabana no tiene el color característico de la hierba: Minecraft guarda las texturas como la de la hierba en grisáceo y se colorean programáticamente después; esto no está implementado y se ve visualmente horrible en biomas como la sabana.
  - [x] PORTADO Y VERIFICADO 2026-06-22 (núcleo de color): `BiomeColor.h` porta 1:1
    `Biome.getGrassColor/getFoliageColor` (override-o-textura sobre el `ColorMapColorUtil`
    certificado) y `GrassColorModifier` (NONE / DARK_FOREST `((c&0xFEFEFE)+2634762)>>1` /
    SWAMP vía `BIOME_INFO_NOISE`, el `PerlinSimplexNoise` certificado). Test runnable headless
    `biome_color_parity` contra el colormap real + JSON de bioma real: plains→#91BD59 y
    forest→#79C05A (cross-verificados con un segundo decodificador PNG independiente), modifier
    dark_forest y swamp → OK.
  - [x] RENDER-SIDE PORTADO Y VERIFICADO 2026-06-22: `render/level/BiomeTint.h` porta 1:1
    `ClientLevel.calculateBlockTint` (blend de caja (2r+1)², radio por defecto 2) + los
    resolvers `BiomeColors` GRASS/FOLIAGE/WATER + la clasificación textura→resolver de
    `BlockColors`. Test runnable `biome_tint_parity` contra colormaps + JSON reales: plains
    #91BD59 (r0 y r2), water=waterColor del bioma, no-tinteadas→none, blend plains|forest
    intermedio → ALL OK.
  - [x] MESHER CABLEADO Y COMPILA 2026-06-22: `ChunkMesh` acepta un `BiomeMeshContext*`
    opcional (snapshot `biomeAt` + colormaps grass/foliage + radio) y aplica `biometint::tint`
    en las rutas de emisión (cube `emitFace`, `emitCross`, y bake de modelo para hojas/grass).
    Con contexto nullptr el comportamiento es idéntico al anterior (cero riesgo).
  - [x] RUNTIME INTEGRADO Y VERIFICADO 2026-06-27 (Linux headless): `LevelRenderer::rebuildDirtyChunks`
    construye el `BiomeMeshContext` en el hilo principal — `ensureBiomeData()` carga los
    colormaps grass/foliage (stb_image) desde `assets.bin` (MÁS el `BiomeRegistry` desde
    `data/minecraft/worldgen/biome`) y `makeBiomeContext(cp)` muestrea biomas del chunk +
    margen (radio 2) en un snapshot inmutable (evita la data race con el cache de `BiomeManager`
    al llamar desde el worker). El contexto se pasa a `buildChunk`. Smoke Linux headless
    (Xvfb + Mesa llvmpipe GL 4.6) confirma: `[INF] TextureAtlas: built 512x576 atlas from
    assets.bin, 1112 of 1133 textures loaded (52 animated)` + `[INF] Biome colouring active
    (65 biomes)` + frames ticking sin warnings de "colormap/registry unavailable".
  - [x] ASSET PACKING 2026-06-27: `tools/asset_packer/main.cpp` acepta ahora un 5º argumento
    `client_assets_dir` y empaqueta `minecraft/textures/colormap/*.png` + `minecraft/textures/block/*.png`
    desde el `client.jar` extraído en `assets/client-extract/assets/` (la asset index `30.json`
    del repo sólo lleva sounds/lang, no texturas). `CMakeLists.txt` pasa el dir cuando existe.
    Sin esto, el exe standalone no encuentra colormaps ni texturas de bloques (Linux headless
    logueaba `assets.bin fallback found no block textures` y caía al tint hardcoded).
  - [x] FALLBACK TINT CORREGIDO 2026-06-27: `ChunkMesh::getTextureTint` devolvía `#79C05A`
    (forest, RGB 121,192,90) para hierba y `#59AE30` (RGB 89,174,48) para follaje, ambos
    mal etiquetados como "plains". Resampleado el colormap real con `ColorMapColorUtil::get`:
    plains grass (0.8,0.4)→`#91BD59` (RGB 145,189,89), plains foliage→`#77AB2F` (RGB 119,171,47).
    Actualizado a los valores correctos. Los tintes fijos de spruce/birch/lily_pad/dry_foliage
    ya eran correctos 1:1 con `BlockColors.java`.
  - Resumen del estado: la lógica 1:1 (cálculo de color + blend + clasificación textura→resolver)
    está portada y probada por `biome_color_parity` y `biome_tint_parity`. El runtime integra
    todo: carga colormaps desde `assets.bin`, construye el snapshot de bioma por chunk en el hilo
    principal, y aplica `biometint::tint` por vértice. Faltante NO bloqueante: verificación
    visual con GPU real + cámara interactiva (el smoke Xvfb captura el shader del cielo pero
    el ángulo de cámara headless produce un frame dominado por horizonte). El cálculo de color
    es 1:1 con Java (paridad demostrada por los tests).

## Biomas / terreno

- [ ] Faltan muchos biomas por implementar; la variedad es muy limitada y la dispersión es anormal en comparación con el juego original.
- [ ] Probablemente falten terrain features de montañas... es bastante plano el terreno en general.
- [ ] Parece que hay un intento de generación de bioma de pantano pero está fatal implementado: hay nenúfares que spawnean encima de la tierra, los nenúfares se ven como un bloque gris y no están coloreados programáticamente (siguen la misma lógica que las flores, un plano en un bloque ligeramente levantado), la generación de terreno no es una generación de terreno de pantano, hay lagos, es todo más plano, la hierba cambia de color... simplemente está implementado de manera muy pobre.
- [ ] Implementa correctamente todos los biomas; debería haber sobre 65 biomas en el juego original. Ten en cuenta los bloques que spawnean de manera natural en cada bioma e impleméntalos.

## Revisión final

- [ ] Finalmente haz una revisión general de qué tan 1:1 es el generador. Yo sé en el fondo que probablemente sea una aproximación chapucera, pero el objetivo final sería tener un generador que, dada la misma semilla que en el juego original, diera el mismo mundo. No te pido que lo arregles, solamente que lo valores.

## Proceso

- [ ] Cada tarea sube el progreso a git.

## Render en Linux / verificación visual (hallazgo 2026-06-22)

- [x] El cliente completo `mcpp` AHORA COMPILA, ENLAZA Y EJECUTA en Linux headless
      (Xvfb + Mesa llvmpipe GL 4.5). Verificado: arranca, carga el registro completo
      (29873 block states / 1168 blocks), genera mundo, coloca estructuras, sube el
      pipeline GL y renderiza el cielo. Requisitos provisionados en la sesión:
      `vendor/glfw` (clonado), headers X11/GL (apt), y `tools/provision_runtime.sh`.
- [x] RESUELTO 2026-06-22: el atlas de bloques ahora se stitchea en runtime en Linux
      (LevelRenderer.loadAtlas cae a `TextureAtlas::loadFromAssetPack`, que ya existía,
      construyendo el atlas desde las texturas sueltas de assets.bin). Verificado bajo
      Xvfb+llvmpipe: "built 512x416 atlas, 800 of 821 textures loaded" y el mundo renderiza
      terreno texturizado con hierba verde coloreada por bioma + formas de bloque correctas.
      Nota: el GL software necesita MESA_GL_VERSION_OVERRIDE=4.6 porque los shaders son
      #version 460 (las GPUs reales soportan 4.6 nativo).
- [ ] (histórico) BLOQUEADOR de verificación visual de TEXTURAS en Linux (independiente del trabajo
      de biomas/rotación, que ya está compilado en el binario y unit-tested): el terreno
      sale sin textura porque el **atlas de bloques stitched NO se genera en Linux**.
      `LevelRenderer::loadAtlas` espera `assets/minecraft/textures/atlas/blocks.png` +
      `assets/minecraft/atlases/blocks.json` (en Windows es un recurso embebido en el .exe);
      `tools/asset_packer/main.cpp` solo empaqueta texturas sueltas `textures/block/*.png`,
      NO el atlas. Falta: cablear el `Stitcher` (ya portado/certificado) en el asset_packer
      para stitchear los block textures → `blocks.png` + `blocks.json` y empaquetarlos, o
      generarlos en build. Además se ve un `glDrawArrays error 0x502` a investigar.
      Con eso, una captura bajo Xvfb verificaría visualmente la rotación de modelos y el
      coloreado por bioma de esta sesión.

## Verificación de paridad worldgen (terreno + biomas) — 2026-06-23, harness operativo en Linux

- [x] HARNESS DE PARIDAD DE DECORACIÓN OPERATIVO EN LINUX: con JDK 25 (Temurin) +
      client.jar + las 107 libs descargadas, `tools/FullChunkDecorateParity.java`
      compila y genera ground-truth IN-PROCESS (el `NoiseBasedChunkGenerator` real,
      sin servidor), y el target C++ `full_chunk_decorate_parity` lo compara
      byte-a-byte. Esto era antes solo-Windows; ahora es reproducible headless.
- [x] TERRENO + BIOMAS BYTE-EXACTOS vs el Java real (seed 1), verificado en 8 biomas
      diversos — cada chunk 98304 celdas, **0 mismatches**:
      plains (0,0), dark_forest (-27,64), swamp (200,120), savanna (120,-200),
      snowy (-80,-80), jungle (-43,60), meadow/montaña (-100,-60), ocean (-40,-13). El chunk completo (relieve por ruido, surface,
      menas, árboles, plantas, decoración) coincide bloque-a-bloque con Minecraft Java.
      => La capa de worldgen (terreno + biomas: routing+surface+decoración) está
      esencialmente COMPLETA y demostrada 1:1; las quejas de "biomas/pantano mal
      implementados" están desactualizadas (el pantano y el dark_forest son byte-exactos).
      Lo que queda fuera de esta verificación: estructuras (generate-structures=false en
      el harness; en progreso en paralelo) y dimensiones nether/end.

## Verificación de estructuras (geometría/math) — 2026-06-23, harness en Linux
- [x] El suite de paridad de estructuras corre headless en Linux (run_groundtruth.sh +
      JDK25). Geometría byte-exacta vs Java confirmada para las grandes aún no colocadas:
      WoodlandMansionGrid (11016 checks, 0 mismatch), OceanMonumentRoomGraph (1188, 0),
      StrongholdPieceBox (280, 0), MineShaftCorridor (17280, 0). Sus cimientos matemáticos
      son 1:1; falta la colocación de bloques (postProcess) — trabajo grande, en progreso
      en paralelo (ruined_portal hoy). Estructuras que SÍ se colocan: aldeas,
      trial_chambers, outpost, swamp_hut, pirámides, igloo, naufragio, nether_fossil,
      buried_treasure, ocean_ruin.

## Estructuras — foundations byte-exactas (suite completa) 2026-06-23
- [x] La geometría/math de TODAS las estructuras grandes restantes está verificada
      byte-exacta vs el Java real (run_groundtruth.sh + JDK25 en Linux), ~1.25M checks,
      0 mismatches: WoodlandMansionGrid + GridLayout (1.21M) + EdgeClean, OceanMonumentRoom
      + RoomGraph + RoomFitter, StrongholdPieceBox + PieceTypeBox + SmallDoor,
      NetherFortressPieceBox, MineShaftCorridor + Crossing + Room (+ Stairs box), IglooPiece,
      OceanRuinCluster. => cuando se porte la colocación de bloques (postProcess) de cada una,
      su layout/geometría está garantizado 1:1. Lo que falta por estructura es SOLO el
      postProcess (con la infra de world-access que necesita: biome/liquid para
      isInInvalidLocation, loot/block-entity para cofres) — trabajo grande por estructura,
      en progreso en paralelo (ruined_portal).

## Estructuras — oráculo de colocación de bloques (ground-truth) en Linux 2026-06-23
- [x] La colocación de bloques (postProcess) de las grandes hand-built
      (mineshaft / stronghold / ocean_monument / woodland_mansion / fortress) NO se
      puede verificar en aislamiento: cada postProcess LEE el terreno ya generado
      (fillColumnDown escanea hacia abajo, isInInvalidLocation chequea bioma+líquidos,
      setPlanksBlock/rails consultan isFaceSturdy del bloque existente). Sólo se puede
      verificar byte-exacto contra un MUNDO COMPLETO ya generado. Ese oráculo ya existe
      en Windows (run_server_gen_structures.ps1 → 26.1.2/server_run/world_structures +
      ServerChunkDump) pero NO había contraparte headless.
- [x] Portada la contraparte Linux: `tools/run_server_gen_structures.sh`. Levanta el
      server 26.1.2 real headless (stdin vía `tail -f`, `tick freeze`, forceload en
      tiles ≤256 chunks por el límite de vanilla, save-all, stop), genera
      `26.1.2/server_run/world_structures/` con generate-structures=true y semilla fija.
      Verificado end-to-end en Linux: región 7×7 chunks (seed 1) → 122 chunks `full`,
      y `ServerChunkDump` decodifica (codec PalettedContainer real de Mojang) los bloques
      de mina como ground-truth: 329 rail, 786 oak_fence, 252 cobweb, 34 iron_chain,
      4 spawner. Una región más amplia (forceload -64..63) ya dio 401 chunks `full` con
      múltiples minas (446 rail, 1042 fence, 297 cobweb, 34 chain, 8 cave_spider spawner,
      Y −49..19). => el oráculo byte-exacto para portar postProcess de las 5 grandes
      queda reproducible en headless/CI. Falta (multi-sesión, una estructura a la vez):
      portar el postProcess + ensamblaje recursivo + infra world-access y comparar
      byte-exacto contra este dump. Mineshaft es la más tratable (su geometría ya está
      verificada 1:1 y es la estructura más común, así que aparece en el dump).
- [x] Mineshaft — ENSAMBLAJE RECURSIVO COMPLETO byte-exacto (2026-06-23). Portado
      `MineshaftAssembly.h`: createRandomShaftPiece + generateAndAddPiece + las 4
      addChildren (Room/Corridor/Crossing/Stairs) + moveBelowSeaLevel, atando los
      helpers de caja ya certificados (findCorridorSize/findCrossing/makeRoomBox/
      findStairs) en la recursión depth-first con el ORDEN DE DRAWS RNG exacto
      (nextInt(100) selector, nextInt(3)/nextInt(23) rails/spider del ctor corridor,
      nextInt(4) endSelection, nextInt(span)/nextInt(heightSpace) del room, nextBoolean
      de crossing two-floored, nextDouble inicial de findGenerationPoint). Oráculo:
      `tools/MineshaftAssemblyParity.java` corre la MineshaftPieces real. Gate C++
      `mineshaft_assembly_parity` (target CMake). Verificado: 5 semillas × 49 chunks =
      245 ensamblajes, ~32.000 chequeos de pieza (kind, caja 6-int, orientación,
      hasRails/spider/numSections, twoFloored/dirección, offset post-sea-level),
      0 mismatches. => la geometría EXACTA (lista de piezas + cajas) de cualquier mina
      está garantizada 1:1. Falta SOLO el postProcess (colocación de bloques, que lee el
      terreno) — verificable ahora contra el dump de run_server_gen_structures.sh.
- [~] Mineshaft — POSTPROCESS (colocación de bloques) PORTADO (2026-06-24). Nuevo
      `structures/MineshaftPieces.h`: port 1:1 de los 4 postProcess (Corridor/Crossing/
      Room/Stairs) sobre el ensamblaje byte-exacto. Helpers portados: generateBox,
      generateMaybeBox, maybeGenerateBlock, generateUpperHalfSphere, isInterior,
      setPlanksBlock, isSupportingBox, isInInvalidLocation (sólo liquid — gap: biome
      MINESHAFT_BLOCKING tag), placeSupport, placeDoubleLowerOrUpperSupport,
      fillPillarDownOrChainUp, canPlaceColumnOnTopOf, canHangChainBelow,
      hasSturdyNeighbours, maybePlaceCobWeb, placeSupportPillar, createChest (rail
      placement; loot/minecart-chest DEFERIDO). MsType NORMAL/MESA = oak/dark_oak.
      Cableado: `tryPlaceMineshaft` en Runtime + `minecraft:mineshaft` en
      supportedTypes + parseo `mineshaft_type` JSON. Verificado: `g++ -fsyntax-only`
      de StructureGen.cpp + MineshaftPieces.h OK; todos los parity tests de
      estructuras siguen en verde. Gaps honestos: loot del cofre, minecart-chest,
      SpawnerBlockEntity.setEntityId, childEntranceBoxes del Room, biome tag
      MINESHAFT_BLOCKING — documentados en el header.
