/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#ifndef _SQL_PARSER_H_
#define _SQL_PARSER_H_

#include "sql_mvc.h"

/* the next define makes the parser output more specific error messages,
 * instead of only a dull 'parse error' */
#define YYERROR_VERBOSE 1
/* enable this to get an idea of what the parser is doing on stdout
#define YYDEBUG 1
*/

typedef enum tokens {
	SQL_CREATE_SCHEMA,
	SQL_CREATE_TABLE,
	SQL_CREATE_VIEW,
	SQL_CREATE_INDEX,
	SQL_CREATE_ROLE,
	SQL_CREATE_USER,
	SQL_CREATE_TYPE,
	SQL_CREATE_FUNC,
	SQL_CREATE_SEQ,
	SQL_CREATE_TRIGGER,
	SQL_DROP_SCHEMA,
	SQL_DROP_TABLE,
	SQL_DROP_VIEW,
	SQL_DROP_INDEX,
	SQL_DROP_ROLE,
	SQL_DROP_USER,
	SQL_DROP_TYPE,
	SQL_DROP_FUNC,
	SQL_DROP_SEQ,
	SQL_DROP_TRIGGER,
	SQL_ALTER_TABLE,
	SQL_ALTER_SEQ,
	SQL_ALTER_USER,
	SQL_RENAME_USER,
	SQL_DROP_COLUMN,
	SQL_DROP_CONSTRAINT,
	SQL_DROP_DEFAULT,
	SQL_ANALYZE,
	SQL_DECLARE,
	SQL_SET,
	SQL_CALL,
	SQL_PREP,
	SQL_NAME,
	SQL_USER,
	SQL_PATH,
	SQL_CHARSET,
	SQL_SCHEMA,
	SQL_TABLE,
	SQL_TABLE_OPERATOR,
	SQL_TYPE,
	SQL_CASE,
	SQL_CAST,
	SQL_RETURN,
	SQL_IF,
	SQL_ELSE,
	SQL_WHILE,
	SQL_COLUMN,
	SQL_COLUMN_OPTIONS,
	SQL_COALESCE,
	SQL_CONSTRAINT,
	SQL_CHECK,
	SQL_DEFAULT,
	SQL_NOT_NULL,
	SQL_NULL,
	SQL_IS_NULL,
	SQL_IS_NOT_NULL,
	SQL_NULLIF,
	SQL_UNIQUE,
	SQL_PRIMARY_KEY,
	SQL_FOREIGN_KEY,
	SQL_BEGIN,
	TR_COMMIT,
	TR_ROLLBACK,
	TR_SAVEPOINT,
	TR_RELEASE,
	TR_START,
	TR_MODE,
	SQL_INSERT,
	SQL_DELETE,
	SQL_UPDATE,
	SQL_STORAGE,
	SQL_CROSS,
	SQL_JOIN,
	SQL_SELECT,
	SQL_CONNECT,
	SQL_DISCONNECT,
	SQL_DATABASE,
	SQL_PORT,
	SQL_WHERE,
	SQL_FROM,
	SQL_UNIONJOIN,
	SQL_UNION,
	SQL_EXCEPT,
	SQL_INTERSECT,
	SQL_VALUES,
	SQL_ASSIGN,
	SQL_ORDERBY,
	SQL_GROUPBY,
	SQL_DESC,
	SQL_AND,
	SQL_OR,
	SQL_NOT,
	SQL_EXISTS,
	SQL_NOT_EXISTS,
	SQL_OP,
	SQL_UNOP,
	SQL_BINOP,
	SQL_NOP,
	SQL_BETWEEN,
	SQL_NOT_BETWEEN,
	SQL_LIKE,
	SQL_NOT_LIKE,
	SQL_IN,
	SQL_NOT_IN,
	SQL_GRANT,
	SQL_GRANT_ROLES,
	SQL_REVOKE,
	SQL_REVOKE_ROLES,
	SQL_EXECUTE,
	SQL_PRIVILEGES,
	SQL_ROLE,
	SQL_PW_UNENCRYPTED,
	SQL_PW_ENCRYPTED,
	SQL_PARAMETER,
	SQL_FUNC,
	SQL_AGGR,
	SQL_RANK,
	SQL_FRAME,
	SQL_COMPARE,
	SQL_FILTER,
	SQL_TEMP_LOCAL,
	SQL_TEMP_GLOBAL,
	SQL_INT_VALUE,
	SQL_ATOM,
	SQL_USING,
	SQL_WHEN,
	SQL_ESCAPE,
	SQL_COPYFROM,
	SQL_BINCOPYFROM,
	SQL_COPYTO,
	SQL_EXPORT,
	SQL_NEXT,
	SQL_MULSTMT,
	SQL_WITH,
	SQL_START,
	SQL_INC,
	SQL_MINVALUE,
	SQL_MAXVALUE,
	SQL_CACHE,
	SQL_CYCLE,
	SQL_XMLCOMMENT,
	SQL_XMLCONCAT,
	SQL_XMLDOCUMENT,
	SQL_XMLELEMENT,
	SQL_XMLATTRIBUTE,
	SQL_XMLFOREST,
	SQL_XMLPARSE,
	SQL_XMLPI,
	SQL_XMLQUERY,
	SQL_XMLTEXT,
	SQL_XMLVALIDATE,
	SQL_XMLNAMESPACES,
	SQL_MATRIX,
	SQL_MATRIXADD,
	SQL_MATRIXSQRT,
	SQL_MATRIXQQR,
	SQL_GATHER,
} tokens;

typedef enum jt {
	jt_inner = 0,
	jt_left = 1,
	jt_right = 2,
	jt_full = 3,
	jt_union = 4
} jt;

extern int find_subgeometry_type(char*);
extern char *token2string(int token);
extern int sqlparse(mvc *m);
extern int sqlerror(mvc *sql, const char *s);

#endif /*_SQL_PARSER_H_*/

