#!/bin/bash
set -e

echo ">>> Loading CSV files..."

# Cambia esto si tus campos tienen comillas
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<EOF

\COPY actor FROM '/data/actor.csv' CSV HEADER;
\COPY address FROM '/data/address.csv' CSV HEADER;
\COPY category FROM '/data/category.csv' CSV HEADER;
\COPY city FROM '/data/city.csv' CSV HEADER;
\COPY country FROM '/data/country.csv' CSV HEADER;
\COPY customer FROM '/data/customer.csv' CSV HEADER;
\COPY film_actor FROM '/data/film_actor.csv' CSV HEADER;
\COPY film_category FROM '/data/film_category.csv' CSV HEADER;
\COPY film FROM '/data/film.csv' CSV HEADER;
\COPY inventory FROM '/data/inventory.csv' CSV HEADER;
\COPY language FROM '/data/language.csv' CSV HEADER;
\COPY payment FROM '/data/payment.csv' CSV HEADER;
\COPY rental FROM '/data/rental.csv' CSV HEADER;
\COPY staff FROM '/data/staff.csv' CSV HEADER;
\COPY store FROM '/data/store.csv' CSV HEADER;

EOF

echo ">>> CSV loaded successfully!"
