CREATE TABLE actor (
    actor_id       INTEGER PRIMARY KEY,
    first_name     TEXT,
    last_name      TEXT,
    last_update    TIMESTAMP
);

CREATE TABLE address (
    address_id     INTEGER PRIMARY KEY,
    address        TEXT,
    address2       TEXT,
    district       TEXT,
    city_id        INTEGER,
    postal_code    TEXT,
    phone          TEXT,
    last_update    TIMESTAMP
);

CREATE TABLE category (
    category_id    INTEGER PRIMARY KEY,
    name           TEXT,
    last_update    TIMESTAMP
);

CREATE TABLE city (
    city_id        INTEGER PRIMARY KEY,
    city           TEXT,
    country_id     INTEGER,
    last_update    TIMESTAMP
);

CREATE TABLE country (
    country_id     INTEGER PRIMARY KEY,
    country        TEXT,
    last_update    TIMESTAMP
);

CREATE TABLE customer (
    customer_id    INTEGER PRIMARY KEY,
    store_id       INTEGER,
    first_name     TEXT,
    last_name      TEXT,
    email          TEXT,
    address_id     INTEGER,
    active         INTEGER,
    create_date    TIMESTAMP,
    last_update    TIMESTAMP
);

CREATE TABLE film (
    film_id              INTEGER PRIMARY KEY,
    title                TEXT,
    description          TEXT,
    release_year         INTEGER,
    language_id          INTEGER,
    original_language_id INTEGER,
    rental_duration      INTEGER,
    rental_rate          NUMERIC,
    length               INTEGER,
    replacement_cost     NUMERIC,
    rating               TEXT,
    special_features     TEXT,
    last_update          TIMESTAMP
);

CREATE TABLE film_actor (
    actor_id     INTEGER,
    film_id      INTEGER,
    last_update  TIMESTAMP
);

CREATE TABLE film_category (
    film_id      INTEGER,
    category_id  INTEGER,
    last_update  TIMESTAMP
);

CREATE TABLE inventory (
    inventory_id INTEGER PRIMARY KEY,
    film_id      INTEGER,
    store_id     INTEGER,
    last_update  TIMESTAMP
);

CREATE TABLE language (
    language_id  INTEGER PRIMARY KEY,
    name         TEXT,
    last_update  TIMESTAMP
);

CREATE TABLE payment (
    payment_id   INTEGER PRIMARY KEY,
    customer_id  INTEGER,
    staff_id     INTEGER,
    rental_id    INTEGER,
    amount       NUMERIC,
    payment_date TIMESTAMP,
    last_update  TIMESTAMP
);

CREATE TABLE rental (
    rental_id    INTEGER PRIMARY KEY,
    rental_date  TIMESTAMP,
    inventory_id INTEGER,
    customer_id  INTEGER,
    return_date  TIMESTAMP,
    staff_id     INTEGER,
    last_update  TIMESTAMP
);

CREATE TABLE staff (
    staff_id     INTEGER PRIMARY KEY,
    first_name   TEXT,
    last_name    TEXT,
    address_id   INTEGER,
    picture      BYTEA,
    email        TEXT,
    store_id     INTEGER,
    active       INTEGER,
    username     TEXT,
    password     TEXT,
    last_update  TIMESTAMP
);

CREATE TABLE store (
    store_id          INTEGER PRIMARY KEY,
    manager_staff_id  INTEGER,
    address_id        INTEGER,
    last_update       TIMESTAMP
);
