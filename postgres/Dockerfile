FROM postgres:14

# Env variables del contenedor
ENV POSTGRES_USER=compass
ENV POSTGRES_PASSWORD=compass
ENV POSTGRES_DB=compassdb

# Copiar SQL y scripts de carga
COPY create_tables.sql /docker-entrypoint-initdb.d/01_create_tables.sql
COPY load_csv.sh       /docker-entrypoint-initdb.d/02_load_csv.sh
