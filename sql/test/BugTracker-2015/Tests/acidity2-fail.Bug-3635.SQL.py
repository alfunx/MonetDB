try:
    from MonetDBtesting import process
except ImportError:
    import process

import sys, time, monetdb.sql, os

def connect(autocommit):
    return monetdb.sql.connect(database = os.getenv('TSTDB'),
                               hostname = '127.0.0.1',
                               port = int(os.getenv('MAPIPORT')),
                               username = 'monetdb',
                               password = 'monetdb',
                               autocommit = autocommit)

def query(conn, sql):
    cur = conn.cursor()
    cur.execute(sql)
    r = cur.fetchall()
    cur.close()
    return r

def run(conn, sql):
    print sql
    r = conn.execute(sql)

# boring setup and schema creation stuff:
c1 = connect(True)
run(c1, 'create table foo (a int)')
run(c1, 'insert into foo values (1),(2),(3)')
run(c1, 'create view bar as select * from foo')

c2 = connect(True)
run(c1, 'start transaction')
run(c1, 'delete from foo')
run(c1, 'drop view bar')
run(c1, 'create view bar as select * from foo')
run(c2, 'create table baz (a int);drop table baz')
try:
    run(c1, 'commit')
    print "shouldn't get here"
except monetdb.exceptions.OperationalError:
    print "commit failed"
    pass

run(c1, 'insert into foo values (4),(5),(6)') # SIGABRT here
run(c1, 'drop view bar')
run(c1, 'drop table foo')
