SELECT
    cu.customer_id,
    cu.first_name,
    cu.last_name,
    f.title,
    l.name AS language,
    c.name AS category,
    r.rental_date
FROM customer AS cu
JOIN rental AS r
    ON cu.customer_id = r.customer_id
JOIN inventory AS i
    ON r.inventory_id = i.inventory_id
JOIN film AS f
    ON i.film_id = f.film_id
JOIN language AS l
    ON f.language_id = l.language_id
JOIN film_category AS fc
    ON f.film_id = fc.film_id
JOIN category AS c
    ON fc.category_id = c.category_id
