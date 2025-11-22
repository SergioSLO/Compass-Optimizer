SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    a.address,
    ci.city,
    co.country
FROM country as co
JOIN city as ci
    ON co.country_id = ci.country_id
JOIN address as a
    ON a.city_id = ci.city_id
JOIN customer as c
    ON c.address_id = a.address_id