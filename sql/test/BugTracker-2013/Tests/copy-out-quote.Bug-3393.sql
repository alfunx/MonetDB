start transaction;
create table table3393 (a clob, b clob);
insert into table3393 values ('one|two', 'three"four');
insert into table3393 values ('five\tsix', 'seven\neight');
insert into table3393 values ('nine,ten', 'eleven\\twelve');
select * from table3393;
copy select * from table3393 into stdout delimiters ',', '\n', '|';
copy select * from table3393 into stdout delimiters '|', '\n', ',';
copy select * from table3393 into stdout;
rollback;