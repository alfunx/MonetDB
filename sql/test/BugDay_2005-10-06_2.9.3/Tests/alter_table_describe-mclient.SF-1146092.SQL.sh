#!/usr/bin/env bash

cat << EOF > .monetdb
user=monetdb
password=monetdb
EOF

Mlog -x "$MTIMEOUT $SQLCLIENT -h$HOST -p$MAPIPORT -d$TSTDB -ftest -i -e < \"$TSTSRCDIR/alter_table_describe.SF-1146092-src.sql\""

rm -f .monetdb
