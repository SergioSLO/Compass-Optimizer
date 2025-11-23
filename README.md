# COMPASS Lite

Implementación autocontenida en C++17 que replica las ideas del paper **COMPASS: Online Sketch-based Query Optimization for In-Memory Databases**. Trabaja directamente sobre archivos CSV, construye Count-Min Sketches para las columnas de `JOIN`, estima cardinalidades y explora órdenes de ejecución tanto **left-deep** como **bushy** (dinámico estilo COMPASS). También permite exportar el plan a `.dot/.png` para la presentación.

## Entorno con Docker

No es necesario instalar toolchains en el host. Desde la carpeta `Compass-Optimizer/`:

```bash
docker build -t compass-lite-dev .
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

Ejemplo completo con exportación (útil para acompañar la demo que muestra todos los joins de `query_3.sql`):

```bash
docker run --rm -v "$PWD":/workspace -w /workspace compass-lite-dev bash -lc "./bin/compass_lite --mode=both --plan-dot=plan.dot --plan-png=plan.png --data-dir Data query/query_3.sql"
```

El archivo DOT/PNG puede usarse en las diapositivas o en el video de la demo para mostrar el árbol resultante.

## Comparar con PostgreSQL

1. Construye y levanta el contenedor descrito en `postgres/README.md`:

```bash
cd postgres
docker build -t compass-postgres .
docker run --rm -d -p 5432:5432 -v "$(pwd)/../Data:/data" --name pg compass-postgres
```

2. Verifica que `psql` pueda conectarse a `postgresql://compass:compass@localhost:5432/compassdb` (y asegúrate de tener Python 3 instalado para correr el script).

3. Usa el script `scripts/compare_plans.py` desde un contenedor Python (no necesitas tener Python ni psql instalados en el host):

```bash
docker run --rm --network=host -e DEBIAN_FRONTEND=noninteractive -v "$PWD":/workspace -w /workspace python:3-slim bash -lc "apt-get update >/dev/null && apt-get install -y -qq postgresql-client >/dev/null && python scripts/compare_plans.py --data-dir Data query/query_3.sql"
```

El script muestra el costo/cardenalidad estimada de COMPASS-lite y el resultado de `EXPLAIN (ANALYZE, FORMAT JSON)` de PostgreSQL (filas estimadas, filas reales y tiempos). Sirve para comparar los árboles/plans que se presentan en la demo. (En Windows PowerShell usa comillas dobles en lugar de simples para la ruta).

## Datos y consultas

- `Data/` contiene la versión reducida de Sakila/IMDB que usamos para las demostraciones.
- `query/` incluye varios ejemplos (`query_1.sql`, `query_2.sql`, `query_3.sql`, `query_4.sql`). Las dos últimas contienen 4–5 joins para probar el planner COMPASS. Se pueden agregar más; el parser soporta cadenas de `JOIN`, alias sencillos y predicados con `=, !=, <, >, <=, >=` conectados mediante `AND`.

## Flujo recomendado para la demo

1. Ejecutar en modo `both` para comparar el orden heurístico vs. DP y mostrar la diferencia en costos/estimaciones frente a la cardinalidad real.
2. Exportar el árbol `plan.png` y usarlo en las diapositivas.
3. Comentar brevemente los valores que entrega el programa (estimaciones, costos acumulados, cardinalidad real) para cubrir la rúbrica de “análisis de resultados”.
