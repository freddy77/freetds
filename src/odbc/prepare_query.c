/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <tdsutil.h>
#include <tds.h>
#include <tdsodbc.h>
#include "prepare_query.h"
#include "convert_sql2string.h"
#include "odbc_util.h"
#include <time.h>
#include <assert.h>
#include <sqlext.h>

static char  software_version[]   = "$Id: prepare_query.c,v 1.6 2002-09-15 16:08:25 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/*
 * Function transformation (from ODBC to Sybase)
 * String functions
 * ASCII(string) -> ASCII(string)
 * BIT_LENGTH(string) -> 8*OCTET_LENGTH(string)
 * CHAR_LENGTH(string_exp) -> CHAR_LENGTH(string_exp)
 * CHARACTER_LENGTH(string_exp) -> CHAR_LENGTH(string_exp)
 * CONCAT(string_exp1, string_exp2) -> string_exp1 + string_exp2
 * DIFFERENCE(string_exp1, string_exp2) -> DIFFERENCE(string_exp1, string_exp2)
 * INSERT(string_exp1, start, length, string_exp2) -> STUFF(sameparams)
 * LCASE(string_exp) -> LOWER(string)
 * LEFT(string_exp, count) -> SUBSTRING(string, 1, count)
 * LENGTH(string_exp) -> CHAR_LENGTH(RTRIM(string_exp))
 * LOCATE(string, string [,start]) -> CHARINDEX(string, string)
 * (SQLGetInfo ritorna non possibile il terzo argomento)
 * LTRIM(String) -> LTRIM(String)
 * OCTET_LENGTH(string_exp) -> OCTET_LENGTH(string_exp)
 * POSITION(character_exp IN character_exp) ???
 * REPEAT(string_exp, count) -> REPLICATE(same)
 * REPLACE(string_exp1, string_exp2, string_exp3) -> ??
 * RIGHT(string_exp, count) -> RIGHT(string_exp, count)
 * RTRIM(string_exp) -> RTRIM(string_exp)
 * SOUNDEX(string_exp) -> SOUNDEX(string_exp)
 * SPACE(count) (ODBC 2.0) -> SPACE(count) (ODBC 2.0)
 * SUBSTRING(string_exp, start, length) -> SUBSTRING(string_exp, start, length)
 * UCASE(string_exp) -> UPPER(string)
 *
 * Numeric
 * Quasi tutte le stesso
 * ATAN2 -> ATN2
 * TRUNCATE -> ??
 */
int prepare_call(struct _hstmt *stmt)
{
        char *s, *d, *p;
        int i, quoted = 0;
        char	quote_char;
	int	nest_syntax = 0;
	/* list of bit, used as stack, is call ? FIXME limites size... */
	unsigned long	is_calls = 0;

        if (stmt->prepared_query)
                s=stmt->prepared_query;
        else if (stmt->query)
                s=stmt->query;
        else
                return SQL_ERROR;

        /* we can do it because result string will be
	 * not bigger than source string */
        d=s;
        while (*s) {
                if (!quoted && (*s=='"' || *s=='\'')) {
                        quoted = 1;
                        quote_char = *s;
                } else if (quoted && *s==quote_char) {
                        quoted = 0;
                }

                if (quoted){
                        *d++=*s++;
		}else if (*s=='{'){
                        char *pcall = strstr(s, "call ");
			++nest_syntax;
			is_calls <<= 1;
                        if (!pcall || ((pcall-s) != 1 && (pcall-s) != 3) ){
				/* syntax is always in the form {type ...} */
				p = strchr(s,' ');
				if (!p) break;
				s = p+1;
                        }else{
                                int len;
                                s++;
                                len = pcall - s;
                                if (*s == '?')
                                        stmt->prepared_query_is_func = 1;
                                memcpy(d, "exec ", 5);
                                d += 5;
                                s = pcall + 5;
				is_calls |= 1;
                        }
               }else if (nest_syntax>0){
			/* do not copy close syntax */
                        if (*s=='}'){
				--nest_syntax;
				is_calls >>= 1;
				++s;
                                continue;
			/* convert parenthesis in call to spaces */
                        }else if ( (is_calls&1) && (*s=='(' || *s==')')){
                                *d++ = ' ';
                                s++;
                        }else{
                                *d++=*s++;
                        }
                }else{
                        *d++=*s++;
                }
        }
        *d='\0';

        return SQL_SUCCESS;
}


static int _get_sql_textsize(struct _sql_param_info *param)
{
	int len;
	switch (param->param_sqltype) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_BIT:
		len = *param->param_lenbind;
		break;
	case SQL_DATE:
		len = 15;
		break;
	case SQL_TIME:
		len = 6;
		break;
	case SQL_TIMESTAMP:
		len = 22;
		break;
	case SQL_NUMERIC:
	case SQL_DECIMAL:
		len = 34;
		break;
	case SQL_INTEGER:
		len = 11;
		break;
	case SQL_SMALLINT:
		len = 6;
		break;
	case SQL_REAL:
		len = 25;
		break;
	case SQL_FLOAT:
	case SQL_DOUBLE:
		len = 54;
		break;
	case SQL_BIGINT:
		len = 20;
		break;
	case SQL_TINYINT:
		len = 4;
		break;
	case SQL_LONGVARCHAR:
	case SQL_LONGVARBINARY:
	default:
		len = -1;
		break;
	}

	return len;
}


static int _get_param_textsize(struct _sql_param_info *param)
{
	int len = 0;

	switch(*(SQLINTEGER*)param->param_lenbind){
	case SQL_NTS:
		len = strlen(param->varaddr)+1;
		break;
	case SQL_NULL_DATA:
		len = 4;
		break;
	case SQL_DEFAULT_PARAM:
	case SQL_DATA_AT_EXEC:
		/* I don't know what to do */
		odbc_LogError("SQL_DEFAULT_PARAM and SQL_DATA_AT_EXEC not supported");
		len = -1;
		break;
	default:
		len = *(SQLINTEGER*)param->param_lenbind;
		if (0>len)
			len = SQL_LEN_DATA_AT_EXEC(len);
		else
			len = _get_sql_textsize(param); 
	}

	return len;
}

static int _calculate_params_size(struct _hstmt *stmt)
{
	int i;
	int len = 0;
	int l;
	struct _sql_param_info *param;
	for (i = stmt->param_count; i>0; --i){
		param = odbc_find_param(stmt, i);
		if (!param)
			return -1;
		l = _get_param_textsize(param);
		if (l<0)
			return -1;
		len += l;
	}

	return len;
}

static int _need_comma(struct _sql_param_info *param)
{
	if (SQL_NULL_DATA==param->param_type)
		return 0;

	switch (param->param_sqltype){
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_DATE:
	case SQL_TIME:
	case SQL_TIMESTAMP:
		return 1;
	}

	return 0;
}

static int _get_len_data_at_exec(struct _sql_param_info *param)
{
	int len = 0;

	switch(*(SQLINTEGER*)param->param_lenbind){
	case SQL_NTS:
	case SQL_NULL_DATA:
	case SQL_DEFAULT_PARAM:
	case SQL_DATA_AT_EXEC:
		len = -1;
		break;
	default:
		len = *(SQLINTEGER*)param->param_lenbind;
		if (0>len)
			len = SQL_LEN_DATA_AT_EXEC(len);
		else
			len = -1;
	}

	return len;
}

static const char comma = '"';

static char *_get_last_comma(char *s, int len)
{
	s += len-1;
	for(;len>0;--len,--s)
		if (comma==*s) return s;

	return 0;
}

static int _fix_commas(char *s, int s_len)
{
	int  commas_cnt = 0;
	int  len;
	int  ret;
	char *pcomma;
	char *buf_beg;
	char *buf_end;

	buf_beg = s;
	buf_end = s + s_len - 1;
	len = s_len;
	while(pcomma = memchr(buf_beg, comma, len)){
		++commas_cnt;
		buf_beg = pcomma + 1;
		len = buf_end-pcomma;
	}
	ret = s_len + commas_cnt;


	buf_end = s + s_len;
	for (len = s_len; pcomma = _get_last_comma(s, len); len = pcomma-s){
		memmove(pcomma+commas_cnt, pcomma, buf_end-pcomma);
		buf_end = pcomma;
		--commas_cnt;
		*(pcomma+commas_cnt) = comma;
	}

	return ret; 
}

static int parse_prepared_query(struct _hstmt *stmt, int start)
{
	char *s, *d;
	int param_num;
	struct _sql_param_info *param;
	TDSLOCINFO *locale;
	TDSCONTEXT *context;
	char quote_char;
	int quoted;
	int len;
	int res;
	int need_comma;

	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;

	if (start){
		s          = stmt->prepared_query;
		d          = stmt->query;
                param_num  = stmt->prepared_query_is_func?1:0;
		quoted     = 0;
	}else{
		/* load prepared_query parameters from stmt */
		s          = stmt->prepared_query_s;
		d          = stmt->prepared_query_d;
		param_num  = stmt->prepared_query_param_num;
		quoted     = stmt->prepared_query_quoted;
		quote_char = stmt->prepared_query_quote_char;
	}

	while (*s) {
		if (!quoted && (*s=='"' || *s=='\'')) {
			quoted = 1;
			quote_char = *s;
		} else if (quoted && *s==quote_char) {
			quoted = 0;
		}
		if (*s=='?' && !quoted) {
			param_num++;

			param = odbc_find_param(stmt, param_num);
			if (!param)
				return SQL_ERROR;

			need_comma = _need_comma(param);
			/* printf("ctype is %d %d %d\n",param->param_type, param->param_bindtype, param->param_sqltype); */

			if (need_comma)
				*d++ = comma;

			if (_get_len_data_at_exec(param)>0){
				/* save prepared_query parameters to stmt */
				stmt->prepared_query_s          = s;
				stmt->prepared_query_d          = d;
				stmt->prepared_query_param_num  = param_num;
				stmt->prepared_query_quoted     = quoted;
				stmt->prepared_query_quote_char = quote_char;
				stmt->prepared_query_need_bytes = _get_len_data_at_exec(param);

				/* stop parsing and ask for a data */
				return SQL_NEED_DATA;
			}

			len = convert_sql2string(context,
				param->param_bindtype,
				param->varaddr,
				-1,
     				d,
				-1,
				*param->param_lenbind);
			if (TDS_FAIL==len)
				return SQL_ERROR;

			if (need_comma)
				len = _fix_commas(d, len);

			d+=len;
			if (need_comma)
				*d++ = comma;
			s++;	
		} else {
			*d++=*s++;	
		}
	}
	*d='\0';
	/* reset prepared_query parameters in stmt
	 * to prevent wrong calls to this function */
	stmt->prepared_query_s          = 0;
		
	return SQL_SUCCESS;
}

int start_parse_prepared_query(struct _hstmt *stmt)
{
	int len;

	if (!stmt->prepared_query)
		return SQL_ERROR;

	len = _calculate_params_size(stmt);
	if (0>len)
		return SQL_ERROR;

	if (SQL_SUCCESS!=odbc_set_stmt_query(stmt, 0, 
		    strlen(stmt->prepared_query)+1
		    +stmt->param_count*2 /* reserve space for "" */
		    +len                 /* reserve space for parameters */
		    +len/2               /* reserve space for " inside strings */
		    ))
		return SQL_ERROR;

	/* set prepared_query parameters in stmt */

	return parse_prepared_query(stmt, 1);
}

int continue_parse_prepared_query(struct _hstmt *stmt,
				  SQLPOINTER DataPtr,
				  SQLINTEGER StrLen_or_Ind)
{
	char *d;
	struct _sql_param_info *param;
	TDSLOCINFO *locale;
	TDSCONTEXT *context;
	int len;
	int need_bytes;

	if (!stmt->prepared_query)
		return SQL_ERROR;

	if (!stmt->prepared_query_s)
		return SQL_ERROR;

	if (stmt->prepared_query_need_bytes<=0)
		return SQL_ERROR;

	context = stmt->hdbc->henv->tds_ctx;
	locale = context->locale;
	param = odbc_find_param(stmt, stmt->prepared_query_param_num);
	if (!param)
		return SQL_ERROR;

	/* load prepared_query parameters from stmt */
	d          = stmt->prepared_query_d;
	need_bytes = stmt->prepared_query_need_bytes;

	if (SQL_NTS==StrLen_or_Ind)
		StrLen_or_Ind = strlen((char*)DataPtr);
	else if (SQL_DEFAULT_PARAM==StrLen_or_Ind)
		/* FIXME: I don't know what to do */
		return SQL_ERROR;

	if (StrLen_or_Ind>need_bytes && SQL_NULL_DATA!=StrLen_or_Ind)
		StrLen_or_Ind = need_bytes;

	/* put parameter into query */
	len = convert_sql2string(context,
		param->param_bindtype,
		DataPtr,
		StrLen_or_Ind,
		d,
		-1,
		StrLen_or_Ind);
	if (TDS_FAIL==len)
		return SQL_ERROR;

	if (_need_comma(param))
		len = _fix_commas(d, len);

	d+=len;

	need_bytes -= StrLen_or_Ind;
	if (StrLen_or_Ind>0 && need_bytes>0){
		/* set prepared_query parameters in stmt */
		stmt->prepared_query_d          = d;
		stmt->prepared_query_need_bytes = need_bytes;

		/* stop parsing and ask more data */
		return SQL_NEED_DATA;
	}

	/* continue parse prepared query */
	if (_need_comma(param))
		*d++ = comma;

	/* set prepared_query parameters in stmt */
	stmt->prepared_query_s++;
	stmt->prepared_query_d          = d;
	stmt->prepared_query_need_bytes = 0;

	/* continue parsing */
	return parse_prepared_query(stmt, 0);
}

