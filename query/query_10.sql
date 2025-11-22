SELECT
    cu.customer_id,
    cu.first_name,
    cu.last_name,
    co.country,
    ci.city,
    a.address,
    s.store_id,
    st.first_name AS staff_first,
    f.title,
    l.name AS language,
    c.name AS category,
    p.amount AS payment_amount,
    r.rental_date
FROM customer AS cu
JOIN address AS a
    ON cu.address_id = a.address_id
JOIN city AS ci
    ON a.city_id = ci.city_id
JOIN country AS co
    ON ci.country_id = co.country_id
JOIN store AS s
    ON cu.store_id = s.store_id
JOIN staff AS st
    ON s.store_id = st.store_id
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
JOIN payment AS p
    ON r.rental_id = p.rental_id

