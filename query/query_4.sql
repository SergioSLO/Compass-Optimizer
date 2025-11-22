SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    a.address,
    ci.city,
    co.country
FROM customer AS c
JOIN address AS a
    ON c.address_id = a.address_id
JOIN city AS ci
    ON a.city_id = ci.city_id
JOIN country AS co
    ON ci.country_id = co.country_id
WHERE co.country = 'Canada'