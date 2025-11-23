# Compass Postgres Setup

Este directorio contiene todo lo necesario para levantar un contenedor Docker con PostgreSQL, crear automáticamente las tablas y cargar los CSV desde el directorio ../Data.

El objetivo es dejar lista la base de datos compassdb para poder conectarte desde herramientas externas como DataGrip, DBeaver, psql, etc.

------------------------------------------------------------

## 1. Requisitos

- Docker instalado  
- Los CSV ubicados en:

Compass-Optimizer/Data/

Debe contener algo como:

Data/
 ├── customers.csv
 ├── orders.csv
 └── ...

------------------------------------------------------------

## 2. Estructura del directorio

Dentro del repositorio:

Compass-Optimizer/
 ├── Data/
 ├── postgres/
 │    ├── Dockerfile
 │    ├── create_tables.sql
 │    └── load_csv.sh
 └── ...

------------------------------------------------------------

## 3. Construir la imagen Docker

Ejecuta dentro del directorio:

Compass-Optimizer/postgres/

Comando:

docker build -t compass-postgres .

------------------------------------------------------------

## 4. Ejecutar contenedor PostgreSQL

Para cualquier plataforma:

```bash
docker run --rm -d -p 5432:5432 -v "$(pwd)/../Data:/data" --name pg compass-postgres
```

> Windows PowerShell: usa comillas dobles `"` (tal como arriba). Windows CMD no soporta `$(pwd)`, se recomienda PowerShell.

El contenedor:
- Crea automáticamente la BD y tablas (via create_tables.sql)
- Ejecuta load_csv.sh para cargar los CSV
- Expone PostgreSQL en localhost:5432

------------------------------------------------------------

## 5. Verificar que el contenedor está corriendo

docker ps

Debe aparecer algo como:

CONTAINER ID   IMAGE              NAME   STATUS
abcd1234       compass-postgres   pg     Up X seconds

------------------------------------------------------------

## 6. Conectarse desde DataGrip o cualquier cliente

Configura la conexión:

Host: localhost  
Port: 5432  
Database: compassdb  
User: compass  
Password: compass  

Si DataGrip te pide driver y aparece el error:

"Driver class 'org.postgresql.Driver' not found"

Presiona "Download Driver Files".

------------------------------------------------------------

## 7. Listar bases de datos dentro de PostgreSQL

SELECT datname FROM pg_database;

Deberías ver:

postgres  
template0  
template1  
compassdb  

------------------------------------------------------------

## 8. Detener el contenedor

docker stop pg

------------------------------------------------------------

## 9. Volver a levantar la BD

docker run --rm -d -p 5432:5432 -v "$(pwd)/../Data:/data" --name pg compass-postgres

------------------------------------------------------------

## 10. Nota importante

Si cambias algo en create_tables.sql o load_csv.sh, debes reconstruir la imagen:

docker build -t compass-postgres .

------------------------------------------------------------

## 11. Listo

Tu base de datos PostgreSQL está lista, con tablas generadas y CSV cargados, accesible desde cualquier cliente externo.

Host: localhost  
Port: 5432  
DB: compassdb  
User: compass  
Pass: compass

------------------------------------------------------------
