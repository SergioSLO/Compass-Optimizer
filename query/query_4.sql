SELECT
    c.customer_id,
    c.first_name,
    s.store_id,
    p.amount,
    r.rental_date
FROM customer AS c
JOIN rental AS r
    ON c.customer_id = r.customer_id
JOIN payment AS p
    ON r.rental_id = p.rental_id
JOIN staff AS s
    ON p.staff_id = s.staff_id
WHERE p.amount > 5
  AND c.active = 1;
