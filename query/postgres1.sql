SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT 
    a.actor_id,
    a.first_name,
    a.last_name,
    fa.film_id
FROM (
    actor AS a
    JOIN film_actor AS fa
        ON a.actor_id::int = fa.actor_id::int
);
