/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * This code was created by Peter Harvey (mostly during Christmas 98/99).
 * This code is LGPL. Please ensure that this message remains in future
 * distributions and uses of this code (thats about all I get out of it).
 * - Peter Harvey pharvey@codebydesign.com
 *
 * This file has been modified for the MonetDB project.  See the file
 * Copyright in this directory for more information.
 */

/**********************************************************************
 * SQLGetTypeInfo()
 * CLI Compliance: ISO 92
 *
 * Author: Martin van Dinther, Sjoerd Mullender
 * Date  : 30 aug 2002
 *
 **********************************************************************/

#include "ODBCGlobal.h"
#include "ODBCStmt.h"
#include <float.h>


#define NCOLUMNS	19

/* column names of result set */
static const char *columnnames[NCOLUMNS] = {
	"type_name",
	"data_type",
	"column_size",
	"literal_prefix",
	"literal_suffix",
	"create_params",
	"nullable",
	"case_sensitive",
	"searchable",
	"unsigned_attribute",
	"fixed_prec_scale",
	"auto_unique_value",
	"local_type_name",
	"minimum_scale",
	"maximum_scale",
	"sql_data_type",
	"sql_datetime_sub",
	"num_prec_radix",
	"interval_precision"
};

/* column types of result set */
static const char *columntypes[NCOLUMNS] = {
	"varchar",
	"smallint",
	"int",
	"varchar",
	"varchar",
	"varchar",
	"smallint",
	"smallint",
	"smallint",
	"smallint",
	"smallint",
	"smallint",
	"varchar",
	"smallint",
	"smallint",
	"smallint",
	"smallint",
	"int",
	"smallint"
};

/* column lengths of result set */
static const int columnlengths[NCOLUMNS] = {
	128,
	5,
	10,
	128,
	128,
	128,
	5,
	5,
	5,
	5,
	5,
	5,
	128,
	5,
	5,
	5,
	5,
	10,
	5
};

static struct types {
	const char *type_name;
	const int data_type;	/* concise type */
	const int column_size;
	const char *literal_prefix;
	const char *literal_suffix;
	const char *create_params;
	const int nullable;	/* NO_NULLS, NULLABLE, NULLABLE_UNKNOWN */
	const int case_sensitive;	/* SQL_FALSE, SQL_TRUE */
	const int searchable;	/* PRED_NONE, PRED_CHAR, PRED_BASIC, SEARCHABLE */
	const int unsigned_attribute;	/* SQL_FALSE, SQL_TRUE, NULL */
	const int fixed_prec_scale;	/* SQL_FALSE, SQL_TRUE */
	const int auto_unique_value;	/* SQL_FALSE, SQL_TRUE, NULL */
	const char *local_type_name;	/* localized version of type_name */
	const int minimum_scale;
	const int maximum_scale;
	const int sql_data_type;
	const int sql_datetime_sub;
	const int num_prec_radix;
	const int interval_precision;
	const char **tuple;
} types[] = {
	/* this table is sorted on the value of data_type and then on
	 * how "close" the type maps to the corresponding ODBC SQL
	 * type (i.e. in the order SQLGetTypeInfo wants it) */
	{
		"uuid",		       /* type_name */
		SQL_GUID,	       /* data_type */
		36,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_GUID,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"character large object", /* type_name */
		SQL_WLONGVARCHAR,      /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_WLONGVARCHAR,      /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"varchar",	       /* type_name */
		SQL_WVARCHAR,	       /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"length",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_WVARCHAR,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"character",	       /* type_name */
		SQL_WCHAR,	       /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"length",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_WCHAR,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"char",		       /* type_name */
		SQL_WCHAR,	       /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"length",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_WCHAR,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"boolean",	       /* type_name */
		SQL_BIT,	       /* data_type */
		1,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_TRUE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_BIT,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"tinyint",	       /* type_name */
		SQL_TINYINT,	       /* data_type */
		2,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_TINYINT,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"bigint",	       /* type_name */
		SQL_BIGINT,	       /* data_type */
		19,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_BIGINT,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"binary large object", /* type_name */
		SQL_LONGVARBINARY,     /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_LONGVARBINARY,     /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"binary large object", /* type_name */
		SQL_VARBINARY,	       /* data_type */
		1000000,	       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"length",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_TRUE,	       /* case_sensitive */
		SQL_SEARCHABLE,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_VARBINARY,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	/* SQL_BINARY */
	{
		"numeric",	       /* type_name */
		SQL_NUMERIC,	       /* data_type */
		19,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision,scale",     /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		19,		       /* maximum_scale */
		SQL_NUMERIC,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"decimal",	       /* type_name */
		SQL_DECIMAL,	       /* data_type */
		19,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision,scale",     /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		19,		       /* maximum_scale */
		SQL_DECIMAL,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"integer",	       /* type_name */
		SQL_INTEGER,	       /* data_type */
		9,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTEGER,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"int",		       /* type_name */
		SQL_INTEGER,	       /* data_type */
		9,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTEGER,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"mediumint",	       /* type_name */
		SQL_INTEGER,	       /* data_type */
		9,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTEGER,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"smallint",	       /* type_name */
		SQL_SMALLINT,	       /* data_type */
		4,		       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_SMALLINT,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		10,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"float",	       /* type_name */
		SQL_FLOAT,	       /* data_type */
		DBL_MANT_DIG,	       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_FLOAT,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		2,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"real",		       /* type_name */
		SQL_REAL,	       /* data_type */
		FLT_MANT_DIG,	       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_REAL,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		2,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"double",	       /* type_name */
		SQL_DOUBLE,	       /* data_type */
		DBL_MANT_DIG,	       /* column_size */
		NULL,		       /* literal_prefix */
		NULL,		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		SQL_FALSE,	       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		SQL_FALSE,	       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_DOUBLE,	       /* sql_data_type */
		-1,		       /* sql_datetime_sub */
		2,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"date",		       /* type_name */
		SQL_TYPE_DATE,	       /* data_type */
		10,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_DATETIME,	       /* sql_data_type */
		SQL_CODE_DATE,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"time",		       /* type_name */
		SQL_TYPE_TIME,	       /* data_type */
		12,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		NULL,		       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_DATETIME,	       /* sql_data_type */
		SQL_CODE_TIME,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"timestamp",	       /* type_name */
		SQL_TYPE_TIMESTAMP,    /* data_type */
		23,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		-1,		       /* minimum_scale */
		-1,		       /* maximum_scale */
		SQL_DATETIME,	       /* sql_data_type */
		SQL_CODE_TIMESTAMP,    /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		-1,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval year",       /* type_name */
		SQL_INTERVAL_YEAR,     /* data_type */
		9,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_YEAR,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		9,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval month",      /* type_name */
		SQL_INTERVAL_MONTH,    /* data_type */
		10,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_MONTH,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		10,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval day",	       /* type_name */
		SQL_INTERVAL_DAY,      /* data_type */
		5,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_DAY,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		5,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval hour",       /* type_name */
		SQL_INTERVAL_HOUR,     /* data_type */
		6,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_HOUR,	       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		6,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval minute",     /* type_name */
		SQL_INTERVAL_MINUTE,   /* data_type */
		8,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_MINUTE,       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		8,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval second",     /* type_name */
		SQL_INTERVAL_SECOND,   /* data_type */
		10,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_SECOND,       /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		10,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval year to month",/* type_name */
		SQL_INTERVAL_YEAR_TO_MONTH,/* data_type */
		12,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_YEAR_TO_MONTH,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		9,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval day to hour",/* type_name */
		SQL_INTERVAL_DAY_TO_HOUR,/* data_type */
		8,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_DAY_TO_HOUR,  /* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		5,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval day to minute",/* type_name */
		SQL_INTERVAL_DAY_TO_MINUTE,/* data_type */
		11,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_DAY_TO_MINUTE,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		5,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval day to second",/* type_name */
		SQL_INTERVAL_DAY_TO_SECOND,/* data_type */
		14,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_DAY_TO_SECOND,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		5,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval hour to minute",/* type_name */
		SQL_INTERVAL_HOUR_TO_MINUTE,/* data_type */
		9,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_HOUR_TO_MINUTE,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		6,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval hour to second",/* type_name */
		SQL_INTERVAL_HOUR_TO_SECOND,/* data_type */
		12,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_HOUR_TO_SECOND,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		6,		       /* interval_precision */
		NULL		       /* tuple */
	},
	{
		"interval minute to second",/* type_name */
		SQL_INTERVAL_MINUTE_TO_SECOND,/* data_type */
		13,		       /* column_size */
		"'",		       /* literal_prefix */
		"'",		       /* literal_suffix */
		"precision",	       /* create_params */
		SQL_NULLABLE,	       /* nullable */
		SQL_FALSE,	       /* case_sensitive */
		SQL_PRED_BASIC,	       /* searchable */
		-1,		       /* unsigned_attribute */
		SQL_FALSE,	       /* fixed_prec_scale */
		-1,		       /* auto_unique_value */
		NULL,		       /* local_type_name */
		0,		       /* minimum_scale */
		0,		       /* maximum_scale */
		SQL_INTERVAL,	       /* sql_data_type */
		SQL_CODE_MINUTE_TO_SECOND,/* sql_datetime_sub */
		-1,		       /* num_prec_radix */
		10,		       /* interval_precision */
		NULL		       /* tuple */
	},
};

/* find some info about a type given the concise type */
const char *
ODBCGetTypeInfo(int concise_type,
		int *data_type,
		int *sql_data_type,
		int *sql_datetime_sub)
{
	struct types *t;

	for (t = types; t < &types[sizeof(types) / sizeof(types[0])]; t++) {
		if (t->data_type == concise_type) {
			if (data_type)
				*data_type = t->data_type;
			if (sql_data_type)
				*sql_data_type = t->sql_data_type;
			if (sql_datetime_sub)
				*sql_datetime_sub = t->sql_datetime_sub;
			return t->type_name;
		}
	}
	return NULL;
}

static SQLRETURN
MNDBGetTypeInfo(ODBCStmt *stmt,
		SQLSMALLINT DataType)
{
	const char **tuples[sizeof(types) / sizeof(types[0])];
	struct types *t;
	int i;

	switch (DataType) {
	case SQL_ALL_TYPES:
	case SQL_CHAR:
	case SQL_NUMERIC:
	case SQL_DECIMAL:
	case SQL_INTEGER:
	case SQL_SMALLINT:
	case SQL_FLOAT:
	case SQL_REAL:
	case SQL_DOUBLE:
	case SQL_DATE:
	case SQL_TIME:
	case SQL_TIMESTAMP:
	case SQL_VARCHAR:
	case SQL_TYPE_DATE:
	case SQL_TYPE_TIME:
	case SQL_TYPE_TIMESTAMP:
	case SQL_LONGVARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
	case SQL_BIGINT:
	case SQL_TINYINT:
	case SQL_BIT:
	case SQL_WCHAR:
	case SQL_WVARCHAR:
	case SQL_WLONGVARCHAR:
	case SQL_GUID:
	case SQL_INTERVAL_YEAR:
	case SQL_INTERVAL_MONTH:
	case SQL_INTERVAL_DAY:
	case SQL_INTERVAL_HOUR:
	case SQL_INTERVAL_MINUTE:
	case SQL_INTERVAL_SECOND:
	case SQL_INTERVAL_YEAR_TO_MONTH:
	case SQL_INTERVAL_DAY_TO_HOUR:
	case SQL_INTERVAL_DAY_TO_MINUTE:
	case SQL_INTERVAL_DAY_TO_SECOND:
	case SQL_INTERVAL_HOUR_TO_MINUTE:
	case SQL_INTERVAL_HOUR_TO_SECOND:
	case SQL_INTERVAL_MINUTE_TO_SECOND:
		break;

	/* some pre ODBC 3.0 data types which can be mapped to ODBC
	 * 3.0 data types */
	case -80:		/* SQL_INTERVAL_YEAR */
		DataType = SQL_INTERVAL_YEAR;
		break;
	case -81:		/* SQL_INTERVAL_YEAR_TO_MONTH */
		DataType = SQL_INTERVAL_YEAR_TO_MONTH;
		break;
	case -82:		/* SQL_INTERVAL_MONTH */
		DataType = SQL_INTERVAL_MONTH;
		break;
	case -83:		/* SQL_INTERVAL_DAY */
		DataType = SQL_INTERVAL_DAY;
		break;
	case -84:		/* SQL_INTERVAL_HOUR */
		DataType = SQL_INTERVAL_HOUR;
		break;
	case -85:		/* SQL_INTERVAL_MINUTE */
		DataType = SQL_INTERVAL_MINUTE;
		break;
	case -86:		/* SQL_INTERVAL_SECOND */
		DataType = SQL_INTERVAL_SECOND;
		break;
	case -87:		/* SQL_INTERVAL_DAY_TO_HOUR */
		DataType = SQL_INTERVAL_DAY_TO_HOUR;
		break;
	case -88:		/* SQL_INTERVAL_DAY_TO_MINUTE */
		DataType = SQL_INTERVAL_DAY_TO_MINUTE;
		break;
	case -89:		/* SQL_INTERVAL_DAY_TO_SECOND */
		DataType = SQL_INTERVAL_DAY_TO_SECOND;
		break;
	case -90:		/* SQL_INTERVAL_HOUR_TO_MINUTE */
		DataType = SQL_INTERVAL_HOUR_TO_MINUTE;
		break;
	case -91:		/* SQL_INTERVAL_HOUR_TO_SECOND */
		DataType = SQL_INTERVAL_HOUR_TO_SECOND;
		break;
	case -92:		/* SQL_INTERVAL_MINUTE_TO_SECOND */
		DataType = SQL_INTERVAL_MINUTE_TO_SECOND;
		break;

	case -95:		/* SQL_UNICODE_CHAR and SQL_UNICODE */
		DataType = SQL_WCHAR;
		break;
	case -96:		/* SQL_UNICODE_VARCHAR */
		DataType = SQL_WVARCHAR;
		break;
	case -97:		/* SQL_UNICODE_LONGVARCHAR */
		DataType = SQL_WLONGVARCHAR;
		break;
	default:
		/* Invalid SQL data type */
		addStmtError(stmt, "HY004", NULL, 0);
		return SQL_ERROR;
	}

	for (t = types, i = 0; t < &types[sizeof(types) / sizeof(types[0])]; t++) {
		assert(t == types || t->data_type >= (t-1)->data_type);
		if (DataType == SQL_ALL_TYPES || DataType == t->data_type) {
			if (t->tuple == NULL) {
				char buf[32];

				t->tuple = malloc(NCOLUMNS * sizeof(*t->tuple));
				t->tuple[0] = t->type_name;

				snprintf(buf, sizeof(buf), "%d", t->data_type);
				t->tuple[1] = strdup(buf);

				if (t->column_size == -1)
					t->tuple[2] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->column_size);
					t->tuple[2] = strdup(buf);
				}
				t->tuple[3] = t->literal_prefix;
				t->tuple[4] = t->literal_suffix;
				t->tuple[5] = t->create_params;

				snprintf(buf, sizeof(buf), "%d", t->nullable);
				t->tuple[6] = strdup(buf);

				snprintf(buf, sizeof(buf),
					 "%d", t->case_sensitive);
				t->tuple[7] = strdup(buf);

				snprintf(buf, sizeof(buf), "%d", t->searchable);
				t->tuple[8] = strdup(buf);

				if (t->unsigned_attribute == -1)
					t->tuple[9] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->unsigned_attribute);
					t->tuple[9] = strdup(buf);
				}
				snprintf(buf, sizeof(buf),
					 "%d", t->fixed_prec_scale);
				t->tuple[10] = strdup(buf);

				if (t->auto_unique_value == -1)
					t->tuple[11] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->auto_unique_value);
					t->tuple[11] = strdup(buf);
				}
				t->tuple[12] = t->local_type_name;

				if (t->minimum_scale == -1)
					t->tuple[13] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->minimum_scale);
					t->tuple[13] = strdup(buf);
				}
				if (t->maximum_scale == -1)
					t->tuple[14] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->maximum_scale);
					t->tuple[14] = strdup(buf);
				}
				snprintf(buf, sizeof(buf),
					 "%d", t->sql_data_type);
				t->tuple[15] = strdup(buf);

				if (t->sql_datetime_sub == -1)
					t->tuple[16] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->sql_datetime_sub);
					t->tuple[16] = strdup(buf);
				}
				if (t->num_prec_radix == -1)
					t->tuple[17] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->num_prec_radix);
					t->tuple[17] = strdup(buf);
				}
				if (t->interval_precision == -1)
					t->tuple[18] = NULL;

				else {
					snprintf(buf, sizeof(buf),
						 "%d", t->interval_precision);
					t->tuple[18] = strdup(buf);
				}
			}
			tuples[i++] = t->tuple;
		}
	}

	mapi_virtual_result(stmt->hdl, NCOLUMNS, columnnames,
			    columntypes, columnlengths, i, tuples);

	return ODBCInitResult(stmt);
}

#ifdef ODBCDEBUG
static char *
translateDataType(SQLSMALLINT DataType)
{
	switch (DataType) {
	case SQL_ALL_TYPES:
		return "SQL_ALL_TYPES";
	case SQL_CHAR:
		return "SQL_CHAR";
	case SQL_NUMERIC:
		return "SQL_NUMERIC";
	case SQL_DECIMAL:
		return "SQL_DECIMAL";
	case SQL_INTEGER:
		return "SQL_INTEGER";
	case SQL_SMALLINT:
		return "SQL_SMALLINT";
	case SQL_FLOAT:
		return "SQL_FLOAT";
	case SQL_REAL:
		return "SQL_REAL";
	case SQL_DOUBLE:
		return "SQL_DOUBLE";
	case SQL_DATE:
		return "SQL_DATE";
	case SQL_TIME:
		return "SQL_TIME";
	case SQL_TIMESTAMP:
		return "SQL_TIMESTAMP";
	case SQL_VARCHAR:
		return "SQL_VARCHAR";
	case SQL_TYPE_DATE:
		return "SQL_TYPE_DATE";
	case SQL_TYPE_TIME:
		return "SQL_TYPE_TIME";
	case SQL_TYPE_TIMESTAMP:
		return "SQL_TYPE_TIMESTAMP";
	case SQL_LONGVARCHAR:
		return "SQL_LONGVARCHAR";
	case SQL_BINARY:
		return "SQL_BINARY";
	case SQL_VARBINARY:
		return "SQL_VARBINARY";
	case SQL_LONGVARBINARY:
		return "SQL_LONGVARBINARY";
	case SQL_BIGINT:
		return "SQL_BIGINT";
	case SQL_TINYINT:
		return "SQL_TINYINT";
	case SQL_BIT:
		return "SQL_BIT";
	case SQL_WCHAR:
		return "SQL_WCHAR";
	case SQL_WVARCHAR:
		return "SQL_WVARCHAR";
	case SQL_WLONGVARCHAR:
		return "SQL_WLONGVARCHAR";
	case SQL_GUID:
		return "SQL_GUID";
	case SQL_INTERVAL_YEAR:
		return "SQL_INTERVAL_YEAR";
	case SQL_INTERVAL_MONTH:
		return "SQL_INTERVAL_MONTH";
	case SQL_INTERVAL_DAY:
		return "SQL_INTERVAL_DAY";
	case SQL_INTERVAL_HOUR:
		return "SQL_INTERVAL_HOUR";
	case SQL_INTERVAL_MINUTE:
		return "SQL_INTERVAL_MINUTE";
	case SQL_INTERVAL_SECOND:
		return "SQL_INTERVAL_SECOND";
	case SQL_INTERVAL_YEAR_TO_MONTH:
		return "SQL_INTERVAL_YEAR_TO_MONTH";
	case SQL_INTERVAL_DAY_TO_HOUR:
		return "SQL_INTERVAL_DAY_TO_HOUR";
	case SQL_INTERVAL_DAY_TO_MINUTE:
		return "SQL_INTERVAL_DAY_TO_MINUTE";
	case SQL_INTERVAL_DAY_TO_SECOND:
		return "SQL_INTERVAL_DAY_TO_SECOND";
	case SQL_INTERVAL_HOUR_TO_MINUTE:
		return "SQL_INTERVAL_HOUR_TO_MINUTE";
	case SQL_INTERVAL_HOUR_TO_SECOND:
		return "SQL_INTERVAL_HOUR_TO_SECOND";
	case SQL_INTERVAL_MINUTE_TO_SECOND:
		return "SQL_INTERVAL_MINUTE_TO_SECOND";
	case -80:
		return "SQL_INTERVAL_YEAR(ODBC2)";
	case -81:
		return "SQL_INTERVAL_YEAR_TO_MONTH(ODBC2)";
	case -82:
		return "SQL_INTERVAL_MONTH(ODBC2)";
	case -83:
		return "SQL_INTERVAL_DAY(ODBC2)";
	case -84:
		return "SQL_INTERVAL_HOUR(ODBC2)";
	case -85:
		return "SQL_INTERVAL_MINUTE(ODBC2)";
	case -86:
		return "SQL_INTERVAL_SECOND(ODBC2)";
	case -87:
		return "SQL_INTERVAL_DAY_TO_HOUR(ODBC2)";
	case -88:
		return "SQL_INTERVAL_DAY_TO_MINUTE(ODBC2)";
	case -89:
		return "SQL_INTERVAL_DAY_TO_SECOND(ODBC2)";
	case -90:
		return "SQL_INTERVAL_HOUR_TO_MINUTE(ODBC2)";
	case -91:
		return "SQL_INTERVAL_HOUR_TO_SECOND(ODBC2)";
	case -92:
		return "SQL_INTERVAL_MINUTE_TO_SECOND(ODBC2)";
	case -95:
		return "SQL_UNICODE_CHAR and SQL_UNICODE(ODBC2)";
	case -96:
		return "SQL_UNICODE_VARCHAR(ODBC2)";
	case -97:
		return "SQL_UNICODE_LONGVARCHAR(ODBC2)";
	default:
		return "unknown";
	}
}
#endif

SQLRETURN SQL_API
SQLGetTypeInfo(SQLHSTMT StatementHandle,
	       SQLSMALLINT DataType)
{
	ODBCStmt *stmt = (ODBCStmt *) StatementHandle;

#ifdef ODBCDEBUG
	ODBCLOG("SQLGetTypeInfo " PTRFMT " %s\n",
		PTRFMTCAST StatementHandle, translateDataType(DataType));
#endif

	if (!isValidStmt(stmt))
		 return SQL_INVALID_HANDLE;

	clearStmtErrors(stmt);

	return MNDBGetTypeInfo(stmt, DataType);
}

SQLRETURN SQL_API
SQLGetTypeInfoA(SQLHSTMT StatementHandle,
		SQLSMALLINT DataType)
{
	return SQLGetTypeInfo(StatementHandle, DataType);
}

SQLRETURN SQL_API
SQLGetTypeInfoW(SQLHSTMT StatementHandle,
		SQLSMALLINT DataType)
{
	ODBCStmt *stmt = (ODBCStmt *) StatementHandle;

#ifdef ODBCDEBUG
	ODBCLOG("SQLGetTypeInfoW " PTRFMT " %s\n",
		PTRFMTCAST StatementHandle, translateDataType(DataType));
#endif

	if (!isValidStmt(stmt))
		 return SQL_INVALID_HANDLE;

	clearStmtErrors(stmt);

	return MNDBGetTypeInfo(stmt, DataType);
}
