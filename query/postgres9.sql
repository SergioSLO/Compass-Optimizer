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
    st.first_name AS staff_first,
    f.title,
    l.name AS language,
    c.name AS category,
    p.amount AS payment_amount,
    r.rental_date
FROM 
(
    (
        (
            (
                city AS ci
                JOIN country AS co
                    ON ci.country_id::int = co.country_id
            )
            JOIN address AS a
                ON a.city_id::int = ci.city_id::int
        )
        JOIN customer AS cu
            ON cu.address_id::int = a.address_id::int
    )
    JOIN 
    (
        store AS s
        JOIN staff AS st
            ON st.store_id::int = s.store_id::int
    )
        ON cu.store_id::int = s.store_id::int
)
JOIN
(
    (
        rental AS r
        JOIN payment AS p
            ON r.rental_id::int = p.rental_id::int
    )
    JOIN 
    (
        (
            film_category AS fc
            JOIN category AS c
                ON fc.category_id::int = c.category_id::int
        )
        JOIN 
        (
            film AS f
            JOIN language AS l
                ON f.language_id::int = l.language_id::int
        )
            ON f.film_id::int = fc.film_id::int
    )
        ON r.inventory_id::int = r.inventory_id::int
)
ON r.inventory_id::int = (
    SELECT inventory.inventory_id FROM inventory WHERE inventory.film_id = f.film_id LIMIT 1
);
