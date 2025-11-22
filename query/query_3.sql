SELECT
    a.actor_id,
    a.first_name,
    f.title,
    c.name AS category_name
FROM actor AS a
JOIN film_actor AS fa
    ON a.actor_id = fa.actor_id
JOIN film AS f
    ON fa.film_id = f.film_id
JOIN film_category AS fc
    ON f.film_id = fc.film_id
JOIN category AS c
    ON fc.category_id = c.category_id
WHERE c.name = 'Action'
  AND f.rating = 'PG';
