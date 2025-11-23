SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    ci.city,
    co.country,
    f.title
FROM (
    (
        (
            (
                city AS ci
                JOIN (
                    SELECT *
                    FROM country
                    WHERE country = 'Canada'        -- FILTER PUSHDOWN
                ) AS co
                    ON ci.country_id::int = co.country_id
            )
            JOIN (
                SELECT *
                FROM address
            ) AS a
                ON a.city_id::int = ci.city_id::int
        )
        JOIN (
            SELECT *
            FROM customer
            WHERE customer_id::int > 10
        ) AS c
            ON c.address_id::int = a.address_id::int
    )
    JOIN (
        SELECT *
        FROM rental
    ) AS r
        ON r.customer_id::int = c.customer_id::int
)
JOIN (
    (
        SELECT *
        FROM inventory
    ) AS i
    JOIN (
        SELECT *
        FROM film
        WHERE film_id::int < 500
    ) AS f
        ON i.film_id::int = f.film_id::int
)
    ON r.inventory_id::int = i.inventory_id::int
WHERE ci.city > 'A'
  AND ci.city < 'B';
