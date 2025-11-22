SELECT
    a.actor_id,
    a.first_name,
    a.last_name,
    f.title
FROM actor AS a
JOIN film_actor AS fa
    ON a.actor_id = fa.actor_id
JOIN film AS f
    ON fa.film_id = f.film_id
WHERE f.rating = 'PG-13';
