SELECT
    f.title,
    COUNT(r.rental_id) AS times_rented
FROM film AS f
JOIN inventory AS i
    ON f.film_id = i.film_id
JOIN rental AS r
    ON i.inventory_id = r.inventory_id
WHERE f.rating = 'PG-13'
