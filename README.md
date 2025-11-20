# 📌 Compass-Lite Query Optimizer

### *A minimal query optimizer prototype inspired by COMPASS (Sketch-Based Cardinality Estimation)*

Este proyecto implementa una versión simplificada del **optimizador de consultas COMPASS**, diseñado originalmente para MapD/OmniSciDB, pero aquí reducido a un prototipo educativo que funciona sobre archivos **CSV** y **consultas SQL** simples.

El objetivo es mostrar:

* Cómo cargar tablas desde archivos CSV
* Cómo parsear una consulta SQL con JOIN
* Cómo aplicar predicados (WHERE)
* Cómo construir sketches Count-Min
* Cómo estimar la **cardinalidad del join** usando sketches (similar a COMPASS)
* Cómo comparar esta estimación con el resultado real
* Cómo producir un **query plan textual**

Este prototipo está pensado para uso académico (Estructuras de Datos Avanzados, Sistemas de Bases de Datos, Query Optimization).

---

# 🚀 ¿Qué es COMPASS?

El paper COMPASS (“**Online Sketch-Based Query Optimization**”) propone reemplazar estadísticas tradicionales (histogramas, muestras, MCV) por **sketches compactos** que permiten estimar cardinalidades de joins:

[
|A \bowtie B| \approx \sum_{i=1}^{d} \sum_{b} \text{sketch}_A[i][b] \cdot \text{sketch}_B[i][b]
]

Ventajas:

* Muy rápido (O(width × depth))
* Baja memoria
* Robusto a cambios de datos
* Ideal para optimización online / en GPU

Nuestro proyecto implementa una versión **mini**, con:

* Sketch tipo **Count-Min**
* Estimación de joins bucket-wise
* Soporte limitado a **JOIN entre 2 tablas**

---

# 📁 Estructura del proyecto

```
Compass-Optimizer/
│
├── data/           # Aquí van los archivos .csv (tablas)
├── query/          # Aquí van las consultas .sql
├── src/
│   ├── BDReader.cpp/h      # Lector de CSV
│   ├── SQLParser.cpp/h     # Parser simple de SQL
│   ├── Compass.cpp/h       # CMSketch + join estimation
│   ├── Optimizer.cpp/h     # Filtros, join real, query plan
│   └── main.cpp            # Orquestación
│
├── Makefile        # Cross-platform: Windows, Linux, NixOS
└── README.md
```

---

# 🧪 Ejemplo de consulta SQL

`query/example.sql`

```sql
SELECT *
FROM customers
JOIN orders ON customers.id = orders.customer_id
WHERE customers.country = 'Peru';
```

---

# 📦 Ejemplo de datos

`data/customers.csv`

```csv
id,name,country
1,Alice,Peru
2,Bob,Chile
3,Carol,Peru
4,David,Mexico
```

`data/orders.csv`

```csv
id,customer_id,amount
10,1,100
11,1,150
12,2,200
13,3,50
14,3,75
15,4,300
```

---

# 🛠️ Cómo compilar (cross-platform)

Este proyecto tiene un Makefile compatible con:

* **Windows** (PowerShell, CMD, MinGW, MSYS)
* **Ubuntu / Debian / Arch / NixOS**
* **MacOS**

### ▶ Compilar todo

```bash
make
```

Esto genera el binario:

```
bin/compass_lite
```

---

# ▶ Ejecutar una consulta

```bash
make run QUERY=query/example.sql
```

Esto ejecuta:

```
./bin/compass_lite data/*.csv query/example.sql
```

---

# 🧼 Limpiar compilación

```bash
make clean
```

Compatible con **Windows** (`rmdir /S /Q`) y **Linux** (`rm -rf`).

---

# 📊 ¿Qué imprime el programa?

El output muestra:

* Tablas leídas
* Predicados aplicados
* Cantidad de filas post-filtro
* Sketches construidos
* Estimación de cardinalidad del join
* Resultado exacto del join
* Un “query plan” completo

Ejemplo:

```
================= COMPASS-lite Query Plan =================
Query: JOIN customers (id)  ⨝  orders (customer_id)

Tables loaded:
  customers : 4 rows
  orders    : 6 rows

Predicates:
  customers.country = 'Peru'

Plan:
  1) Scan customers (after filters: 2 rows)
  2) Scan orders (after filters: 6 rows)
  3) Build sketches on join keys
  4) Estimate join cardinality
  5) Execute real join for comparison

Results:
  Real join cardinality       : 4
  Estimated join cardinality  : 4.35
===========================================================
```

---

# 🌱 Extensiones futuras

* Soporte para **más de 2 tablas**
* Optimización de orden de joins (DP estilo Selinger)
* Más operadores (>, <, !=)
* Visualización del plan
* Implementación de HyperLogLog para filtros
* Versiones GPU con CUDA

---

# 🎓 Créditos

Proyecto académico inspirado en:

* **COMPASS: Online Sketch-Based Query Optimization for In-Memory Databases**
* OmniSciDB / MapD engine internals
* Query optimization principles (Selinger 1979)

Desarrollado por: **EDA uwu – UTEC**
Curso: *Estructura de Datos Avanzados*