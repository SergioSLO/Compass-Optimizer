SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT 
    f.film_id,
    f.title,
    l.name AS language
FROM (
    SELECT *
    FROM film
    WHERE length > 120
) AS f
JOIN language AS l
    ON f.language_id::int = l.language_id::int;
