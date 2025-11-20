SELECT *
FROM customers
JOIN orders ON customers.id = orders.customer_id
WHERE customers.country = 'Peru';
