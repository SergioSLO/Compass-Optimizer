SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT
    f.title,
    a.first_name,
    a.last_name,
    c.name AS category
FROM (
    film_actor AS fa
    JOIN actor AS a
        ON fa.actor_id = a.actor_id
)
JOIN (
    film AS f
    JOIN (
        film_category AS fc
        JOIN (
            SELECT *
            FROM category
            WHERE name = 'Comedy'
        ) AS c
            ON fc.category_id::int = c.category_id::int
    ) ON f.film_id::int = fc.film_id::int
) ON fa.film_id::int = f.film_id::int;
