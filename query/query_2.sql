SELECT 
    f.film_id,
    f.title,
    l.name AS language
FROM film AS f
JOIN language AS l
    ON f.language_id = l.language_id
WHERE f.length > 120