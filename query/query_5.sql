SELECT
    c.customer_id,
    c.first_name,
    c.last_name,
    SUM(p.amount) AS total_paid
FROM customer AS c
JOIN payment AS p
    ON c.customer_id = p.customer_id
