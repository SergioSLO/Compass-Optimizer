SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT
    r.rental_id,
    f.title,
    c.name AS category,
    r.rental_date
FROM rental AS r
JOIN (
    inventory AS i
    JOIN (
        film AS f
        JOIN (
            film_category AS fc
            JOIN category AS c
                ON fc.category_id::int = c.category_id::int
        ) ON f.film_id::int = fc.film_id::int
    ) ON i.film_id::int = f.film_id::int
) ON r.inventory_id::int = i.inventory_id::int;
