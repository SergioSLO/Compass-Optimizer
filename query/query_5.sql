SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    ci.city,
    co.country,
    f.title
FROM customer AS c
JOIN address AS a
    ON c.address_id = a.address_id
JOIN city AS ci
    ON a.city_id = ci.city_id
JOIN country AS co
    ON ci.country_id = co.country_id
JOIN rental AS r
    ON c.customer_id = r.customer_id
JOIN inventory AS i
    ON r.inventory_id = i.inventory_id
JOIN film AS f
    ON i.film_id = f.film_id
WHERE co.country = 'Canada'
  AND ci.city > 'A'
  AND ci.city < 'B'
  AND f.film_id < 500
  AND c.customer_id > 10;
