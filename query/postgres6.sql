SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT *
FROM (
    (
        SELECT *
        FROM film
        WHERE rating = 'PG-13'
    ) AS f
    JOIN inventory AS i
        ON f.film_id::int = i.film_id::int
)
JOIN rental AS r
    ON i.inventory_id::int = r.inventory_id::int;
