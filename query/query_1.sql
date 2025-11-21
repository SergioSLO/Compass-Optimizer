SELECT 
    a.actor_id,
    a.first_name,
    a.last_name,
    fa.film_id
FROM actor AS a
JOIN film_actor AS fa
    ON a.actor_id = fa.actor_id
