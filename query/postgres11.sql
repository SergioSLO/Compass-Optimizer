SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT
    cu.customer_id,
    cu.first_name,
    cu.last_name,
    co.country,
    ci.city,
    a.address,
    s.store_id,
    st.staff_id,
    st.first_name AS staff_first,
    st.last_name AS staff_last,
    f.film_id,
    f.title,
    l.name AS language,
    c.name AS category,
    ac.first_name AS actor_first,
    ac.last_name AS actor_last,
    p.amount AS payment_amount,
    r.rental_date
FROM (
    rental AS r
    JOIN inventory AS i
        ON r.inventory_id::int = i.inventory_id::int
    JOIN (
        film AS f
        JOIN (
            film_actor AS fa
            JOIN actor AS ac
                ON fa.actor_id::int = ac.actor_id::int
        ) ON f.film_id::int = fa.film_id::int
        JOIN language AS l
            ON f.language_id::int = l.language_id::int
        JOIN (
            film_category AS fc
            JOIN category AS c
                ON fc.category_id::int = c.category_id::int
        ) ON f.film_id::int = fc.film_id::int
    ) ON i.film_id::int = f.film_id::int
)
JOIN (
    customer AS cu
    JOIN (
        address AS a
        JOIN (
            city AS ci
            JOIN country AS co
                ON ci.country_id::int = co.country_id::int
        ) ON a.city_id::int = ci.city_id::int
    ) ON cu.address_id::int = a.address_id::int
) ON r.customer_id::int = cu.customer_id::int
JOIN (
    store AS s
    JOIN staff AS st
        ON st.store_id::int = s.store_id::int
) ON cu.store_id::int = s.store_id::int
JOIN payment AS p
    ON r.rental_id::int = p.rental_id::int;
