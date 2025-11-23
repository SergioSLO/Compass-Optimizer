SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT 
    f.title,
    c.name AS category
FROM film AS f
JOIN (
    film_category AS fc
    JOIN category AS c
        ON fc.category_id::int = c.category_id::int
) ON f.film_id::int = fc.film_id::int;
