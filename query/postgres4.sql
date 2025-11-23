SET join_collapse_limit = 1;
SET from_collapse_limit = 1;

EXPLAIN (ANALYZE, FORMAT JSON)
SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    a.address,
    ci.city,
    co.country,
    f.title
FROM (
    (
        (
            customer AS c
            JOIN (
                address AS a
                JOIN (
                    city AS ci
                    JOIN country AS co
                        ON ci.country_id::int = co.country_id::int
                ) ON a.city_id::int = ci.city_id::int
            ) ON c.address_id::int = a.address_id::int
        )
        JOIN rental AS r
            ON r.customer_id::int = c.customer_id::int
    )
    JOIN (
        inventory AS i
        JOIN film AS f
            ON i.film_id::int = f.film_id::int
    ) ON r.inventory_id::int = i.inventory_id::int
);
