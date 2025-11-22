# COMPASS Lite

Implementación autocontenida en C++17 que replica las ideas del paper **COMPASS: Online Sketch-based Query Optimization for In-Memory Databases**. Trabaja directamente sobre archivos CSV, construye Count-Min Sketches para las columnas de `JOIN`, estima cardinalidades y explora órdenes de ejecución tanto **left-deep** como **bushy** (dinámico estilo COMPASS). También permite exportar el plan a `.dot/.png` para la presentación.

## Entorno con Docker

No es necesario instalar toolchains en el host. Desde la carpeta `Compass-Optimizer/`:

```bash
# 1) construir la imagen con g++, make y graphviz
docker build -t compass-lite-dev .

# 2) compilar dentro del contenedor (monta el repo actual)
docker run --rm -it -v "$PWD":/workspace -w /workspace compass-lite-dev bash -lc "make"
```

> Nota: PowerShell también expone la ruta actual en `$PWD`, por lo que estos comandos funcionan igual en Linux/macOS y en Windows.

El binario resultante queda en `bin/compass_lite` dentro del host. Cuando se vuelva a modificar el código, basta con repetir el segundo comando (no hace falta reconstruir la imagen salvo que se cambie el Dockerfile).

## Ejecución de pruebas

El programa se ejecuta dentro del contenedor (de esa forma evitamos problemas como el `stub-ld` de NixOS). Los siguientes comandos funcionan igual en bash/zsh y en PowerShell (ambos exponen la ruta actual como `$PWD`):

```bash
docker run --rm -v "$PWD":/workspace -w /workspace compass-lite-dev bash -lc "./bin/compass_lite Data/actor.csv Data/film_actor.csv query/query_1.sql"
```

También puedes indicar un directorio y dejar que el programa cargue automáticamente todos los CSV requeridos por la query (ideal para las consultas más grandes `query_3.sql`, `query_4.sql`):

```bash
docker run --rm -v "$PWD":/workspace -w /workspace compass-lite-dev bash -lc "./bin/compass_lite --data-dir Data query/query_2.sql"
```

> Consejo: usa `--data-dir Data` para queries grandes (`query_3.sql`, `query_4.sql`) y evita listar manualmente cada CSV. El programa cargará `Data/<tabla>.csv` según los nombres usados en el SQL.

Parámetros adicionales:

- `--mode=greedy | compass | both` &rarr; selecciona si se imprime el plan left-deep, el DP estilo COMPASS o ambos (por defecto `both`).
- `--plan-dot=plan.dot` &rarr; exporta el árbol del planner COMPASS a Graphviz.
- `--plan-png=plan.png` &rarr; genera PNG (requiere haber pasado `--plan-dot`).
- `--data-dir=Data` &rarr; intenta cargar cada tabla mencionada en el SQL como `<Data>/<tabla>.csv`.

Ejemplo completo con exportación (útil para acompañar la demo con `query_3.sql` o `query_4.sql`):

```bash
docker run --rm -v "$PWD":/workspace -w /workspace compass-lite-dev bash -lc "./bin/compass_lite --mode=both --plan-dot=plan.dot --plan-png=plan.png Data/actor.csv Data/film_actor.csv query/query_1.sql"
```

El archivo DOT/PNG puede usarse en las diapositivas o en el video de la demo para mostrar el árbol resultante.

## Datos y consultas

- `Data/` contiene la versión reducida de Sakila/IMDB que usamos para las demostraciones.
- `query/` incluye varios ejemplos (`query_1.sql`, `query_2.sql`, `query_3.sql`, `query_4.sql`). Las dos últimas contienen 4–5 joins para probar el planner COMPASS. Se pueden agregar más; el parser soporta cadenas de `JOIN`, alias sencillos y predicados con `=, !=, <, >, <=, >=` conectados mediante `AND`.

## Flujo recomendado para la demo

1. Ejecutar en modo `both` para comparar el orden heurístico vs. DP y mostrar la diferencia en costos/estimaciones frente a la cardinalidad real.
2. Exportar el árbol `plan.png` y usarlo en las diapositivas.
3. Comentar brevemente los valores que entrega el programa (estimaciones, costos acumulados, cardinalidad real) para cubrir la rúbrica de “análisis de resultados”.
