CREATE TABLE bar (name CHAR(8), ct INTEGER, ct2 INTEGER);
INSERT INTO bar VALUES ('A', 123, 456);
CREATE TABLE foo AS SELECT SUM(ct + ct2) / 100.0 AS eur FROM bar GROUP BY name WITH DATA;
\d foo

DROP TABLE foo;
DROP TABLE bar;
