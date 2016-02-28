/*
** 2015-04-06
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This is a utility program that computes the differences in content
** between two SQLite databases.
**
** To compile, simply link against SQLite.
**
** See the showHelp() routine below for a brief description of how to
** run the utility.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "mob.h"

/* Safely quote an SQL identifier.  Use the minimum amount of transformation
** necessary to allow the string to be used with %s.
**
** Space to hold the returned string is obtained from sqlite3_malloc().  The
** caller is responsible for ensuring this space is freed when no longer
** needed.
*/
static char *safeId(const char *zId){
	/* All SQLite keywords, in alphabetical order */
	static const char *azKeywords[] = {
		"ABORT", "ACTION", "ADD", "AFTER", "ALL", "ALTER", "ANALYZE", "AND", "AS",
		"ASC", "ATTACH", "AUTOINCREMENT", "BEFORE", "BEGIN", "BETWEEN", "BY",
		"CASCADE", "CASE", "CAST", "CHECK", "COLLATE", "COLUMN", "COMMIT",
		"CONFLICT", "CONSTRAINT", "CREATE", "CROSS", "CURRENT_DATE",
		"CURRENT_TIME", "CURRENT_TIMESTAMP", "DATABASE", "DEFAULT", "DEFERRABLE",
		"DEFERRED", "DELETE", "DESC", "DETACH", "DISTINCT", "DROP", "EACH",
		"ELSE", "END", "ESCAPE", "EXCEPT", "EXCLUSIVE", "EXISTS", "EXPLAIN",
		"FAIL", "FOR", "FOREIGN", "FROM", "FULL", "GLOB", "GROUP", "HAVING", "IF",
		"IGNORE", "IMMEDIATE", "IN", "INDEX", "INDEXED", "INITIALLY", "INNER",
		"INSERT", "INSTEAD", "INTERSECT", "INTO", "IS", "ISNULL", "JOIN", "KEY",
		"LEFT", "LIKE", "LIMIT", "MATCH", "NATURAL", "NO", "NOT", "NOTNULL",
		"NULL", "OF", "OFFSET", "ON", "OR", "ORDER", "OUTER", "PLAN", "PRAGMA",
		"PRIMARY", "QUERY", "RAISE", "RECURSIVE", "REFERENCES", "REGEXP",
		"REINDEX", "RELEASE", "RENAME", "REPLACE", "RESTRICT", "RIGHT",
		"ROLLBACK", "ROW", "SAVEPOINT", "SELECT", "SET", "TABLE", "TEMP",
		"TEMPORARY", "THEN", "TO", "TRANSACTION", "TRIGGER", "UNION", "UNIQUE",
		"UPDATE", "USING", "VACUUM", "VALUES", "VIEW", "VIRTUAL", "WHEN", "WHERE",
		"WITH", "WITHOUT",
	};
	int lwr, upr, mid, c, i, x;
	if (zId[0] == 0) return sqlite3_mprintf("\"\"");
	for (i = x = 0; (c = zId[i]) != 0; i++){
		if (!isalpha(c) && c != '_'){
			if (i>0 && isdigit(c)){
				x++;
			}
			else{
				return sqlite3_mprintf("\"%w\"", zId);
			}
		}
	}
	if (x) return sqlite3_mprintf("%s", zId);
	lwr = 0;
	upr = sizeof(azKeywords) / sizeof(azKeywords[0]) - 1;
	while (lwr <= upr){
		mid = (lwr + upr) / 2;
		c = sqlite3_stricmp(azKeywords[mid], zId);
		if (c == 0) return sqlite3_mprintf("\"%w\"", zId);
		if (c<0){
			lwr = mid + 1;
		}
		else{
			upr = mid - 1;
		}
	}
	return sqlite3_mprintf("%s", zId);
}

/*
** Initialize a Str object
*/
void strInit(Str *p)
{
	p->z = 0;
	p->nAlloc = 0;
	p->nUsed = 0;
}

/*
** Free all memory held by a Str object
*/
void strFree(Str *p)
{
	sqlite3_free(p->z);
	strInit(p);
}

/*
** Add formatted text to the end of a Str object
*/
int strPrintf(Str *p, const char *zFormat, ...)
{
	int nNew;
	for (;;){
		if (p->z){
			va_list ap;
			va_start(ap, zFormat);
			sqlite3_vsnprintf(p->nAlloc - p->nUsed, p->z + p->nUsed, zFormat, ap);
			va_end(ap);
			nNew = (int)strlen(p->z + p->nUsed);
		}
		else{
			nNew = p->nAlloc;
		}
		if (p->nUsed + nNew < p->nAlloc - 1){
			p->nUsed += nNew;
			break;
		}
		p->nAlloc = p->nAlloc * 2 + 1000;
		p->z = sqlite3_realloc(p->z, p->nAlloc);
		if (!p->z) break;
	}
	return !!p->z;
}

/*
** Prepare a new SQL statement.  Print an error and abort if anything
** goes wrong.
*/
static sqlite3_stmt *db_vprepare(sqlite3 * pDB, const char *zFormat, va_list ap){
	char *zSql;
	sqlite3_stmt *pStmt = 0;

	zSql = sqlite3_vmprintf(zFormat, ap);
	if (zSql) {
		sqlite3_prepare_v2(pDB, zSql, -1, &pStmt, 0);
		sqlite3_free(zSql);
	}
	return pStmt;
}

sqlite3_stmt *db_prepare(sqlite3 * pDB, const char *zFormat, ...){
	va_list ap;
	sqlite3_stmt *pStmt;
	va_start(ap, zFormat);
	pStmt = db_vprepare(pDB, zFormat, ap);
	va_end(ap);
	return pStmt;
}

/*
** Free a list of strings
*/
static void namelistFree(char **az){
	if (az){
		int i;
		for (i = 0; az[i]; i++) sqlite3_free(az[i]);
		sqlite3_free(az);
	}
}

/*
** Return a list of column names for the table zDb.zTab.  Space to
** hold the list is obtained from sqlite3_malloc() and should released
** using namelistFree() when no longer needed.
**
** Primary key columns are listed first, followed by data columns.
** The number of columns in the primary key is returned in *pnPkey.
**
** Normally, the "primary key" in the previous sentence is the true
** primary key - the rowid or INTEGER PRIMARY KEY for ordinary tables
** or the declared PRIMARY KEY for WITHOUT ROWID tables.  However, if
** the g.bSchemaPK flag is set, then the schema-defined PRIMARY KEY is
** used in all cases.  In that case, entries that have NULL values in
** any of their primary key fields will be excluded from the analysis.
**
** If the primary key for a table is the rowid but rowid is inaccessible,
** then this routine returns a NULL pointer.
**
** Examples:
**    CREATE TABLE t1(a INT UNIQUE, b INTEGER, c TEXT, PRIMARY KEY(c));
**    *pnPKey = 1;
**    az = { "rowid", "a", "b", "c", 0 }  // Normal case
**    az = { "c", "a", "b", 0 }           // g.bSchemaPK==1
**
**    CREATE TABLE t2(a INT UNIQUE, b INTEGER, c TEXT, PRIMARY KEY(b));
**    *pnPKey = 1;
**    az = { "b", "a", "c", 0 }
**
**    CREATE TABLE t3(x,y,z,PRIMARY KEY(y,z));
**    *pnPKey = 1                         // Normal case
**    az = { "rowid", "x", "y", "z", 0 }  // Normal case
**    *pnPKey = 2                         // g.bSchemaPK==1
**    az = { "y", "x", "z", 0 }           // g.bSchemaPK==1
**
**    CREATE TABLE t4(x,y,z,PRIMARY KEY(y,z)) WITHOUT ROWID;
**    *pnPKey = 2
**    az = { "y", "z", "x", 0 }
**
**    CREATE TABLE t5(rowid,_rowid_,oid);
**    az = 0     // The rowid is not accessible
*/
static char **columnNames(sqlite3 * pDB,
	const char *zDb,                /* Database ("main" or "aux") to query */
	const char *zTab,               /* Name of table to return details of */
	int *pnPKey,                    /* OUT: Number of PK columns */
	int *pbRowid                    /* OUT: True if PK is an implicit rowid */
	){
	char **az = 0;           /* List of column names to be returned */
	int naz = 0;             /* Number of entries in az[] */
	sqlite3_stmt *pStmt;     /* SQL statement being run */
	char *zPkIdxName = 0;    /* Name of the PRIMARY KEY index */
	int truePk = 0;          /* PRAGMA table_info indentifies the PK to use */
	int nPK = 0;             /* Number of PRIMARY KEY columns */
	int i, j;                /* Loop counters */

	/* Normal case:  Figure out what the true primary key is for the table.
	**   *  For WITHOUT ROWID tables, the true primary key is the same as
	**      the schema PRIMARY KEY, which is guaranteed to be present.
	**   *  For rowid tables with an INTEGER PRIMARY KEY, the true primary
	**      key is the INTEGER PRIMARY KEY.
	**   *  For all other rowid tables, the rowid is the true primary key.
	*/
pStmt = db_prepare(pDB, "PRAGMA %s.index_list=%Q", zDb, zTab);
	while (SQLITE_ROW == sqlite3_step(pStmt)){
		if (sqlite3_stricmp((const char*)sqlite3_column_text(pStmt, 3), "pk") == 0){
			zPkIdxName = sqlite3_mprintf("%s", sqlite3_column_text(pStmt, 1));
			break;
		}
	}
	sqlite3_finalize(pStmt);
	if (zPkIdxName){
		int nKey = 0;
		int nCol = 0;
		truePk = 0;
		pStmt = db_prepare(pDB, "PRAGMA %s.index_xinfo=%Q", zDb, zPkIdxName);
		while (SQLITE_ROW == sqlite3_step(pStmt)){
			nCol++;
			if (sqlite3_column_int(pStmt, 5)){ nKey++; continue; }
			if (sqlite3_column_int(pStmt, 1) >= 0) truePk = 1;
		}
		if (nCol == nKey) truePk = 1;
		if (truePk){
			nPK = nKey;
		}
		else{
			nPK = 1;
		}
		sqlite3_finalize(pStmt);
		sqlite3_free(zPkIdxName);
	}
	else{
		truePk = 1;
		nPK = 1;
	}
	pStmt = db_prepare(pDB, "PRAGMA %s.table_info=%Q", zDb, zTab);

	*pnPKey = nPK;
	naz = nPK;
	az = sqlite3_malloc(sizeof(char*)*(nPK + 1));
	if (az == 0) return 0;
	memset(az, 0, sizeof(char*)*(nPK + 1));
	while (SQLITE_ROW == sqlite3_step(pStmt)){
		int iPKey;
		if (truePk && (iPKey = sqlite3_column_int(pStmt, 5))>0){
			az[iPKey - 1] = safeId((char*)sqlite3_column_text(pStmt, 1));
		}
		else{
			az = sqlite3_realloc(az, sizeof(char*)*(naz + 2));
			if (az == 0) return 0;
			az[naz++] = safeId((char*)sqlite3_column_text(pStmt, 1));
		}
	}
	sqlite3_finalize(pStmt);
	if (az) az[naz] = 0;

	/* If it is non-NULL, set *pbRowid to indicate whether or not the PK of
	** this table is an implicit rowid (*pbRowid==1) or not (*pbRowid==0).  */
	if (pbRowid) *pbRowid = (az[0] == 0);

	/* If this table has an implicit rowid for a PK, figure out how to refer
	** to it. There are three options - "rowid", "_rowid_" and "oid". Any
	** of these will work, unless the table has an explicit column of the
	** same name.  */
	if (az[0] == 0){
		const char *azRowid[] = { "rowid", "_rowid_", "oid" };
		for (i = 0; i<sizeof(azRowid) / sizeof(azRowid[0]); i++){
			for (j = 1; j<naz; j++){
				if (sqlite3_stricmp(az[j], azRowid[i]) == 0) break;
			}
			if (j >= naz){
				az[0] = sqlite3_mprintf("%s", azRowid[i]);
				break;
			}
		}
		if (az[0] == 0){
			for (i = 1; i<naz; i++) sqlite3_free(az[i]);
			sqlite3_free(az);
			az = 0;
		}
	}
	return az;
}

/*
** Print the sqlite3_value X as an SQL literal.
*/
static void printQuoted(Str *out, sqlite3_value *X){
	switch (sqlite3_value_type(X)){
	case SQLITE_FLOAT: {
		double r1;
		char zBuf[50];
		r1 = sqlite3_value_double(X);
		sqlite3_snprintf(sizeof(zBuf), zBuf, "%!.15g", r1);
		strPrintf(out, "%s", zBuf);
		break;
	}
	case SQLITE_INTEGER: {
		strPrintf(out, "%lld", sqlite3_value_int64(X));
		break;
	}
	case SQLITE_BLOB: {
		const unsigned char *zBlob = sqlite3_value_blob(X);
		int nBlob = sqlite3_value_bytes(X);
		if (zBlob){
			int i;
			strPrintf(out, "x'");
			for (i = 0; i<nBlob; i++){
				strPrintf(out, "%02x", zBlob[i]);
			}
			strPrintf(out, "'");
		}
		else{
			strPrintf(out, "NULL");
		}
		break;
	}
	case SQLITE_TEXT: {
		const unsigned char *zArg = sqlite3_value_text(X);
		int i, j;

		if (zArg == 0){
			strPrintf(out, "NULL");
		}
		else{
			strPrintf(out, "'");
			for (i = j = 0; zArg[i]; i++){
				if (zArg[i] == '\''){
					strPrintf(out, "%.*s'", i - j + 1, &zArg[j]);
					j = i + 1;
				}
			}
			strPrintf(out, "%s'", &zArg[j]);
		}
		break;
	}
	case SQLITE_NULL: {
		strPrintf(out, "NULL");
		break;
	}
	}
}

/*
** Output SQL that will recreate the aux.zTab table.
*/
static void dump_table(sqlite3 * pDB, const char *zAux, const char *zTab, Str *out){
	char *zId = safeId(zTab); /* Name of the table */
	char **az = 0;            /* List of columns */
	int nPk;                  /* Number of true primary key columns */
	int nCol;                 /* Number of data columns */
	int i;                    /* Loop counter */
	sqlite3_stmt *pStmt;      /* SQL statement */
	const char *zSep;         /* Separator string */
	Str ins;                  /* Beginning of the INSERT statement */

	pStmt = db_prepare(pDB, "SELECT sql FROM %Q.sqlite_master WHERE name=%Q", zAux, zTab);
	if (SQLITE_ROW == sqlite3_step(pStmt)){
		strPrintf(out, "%s;\n", sqlite3_column_text(pStmt, 0));
	}
	sqlite3_finalize(pStmt);
//	if (!g.bSchemaOnly){
	az = columnNames(pDB, zAux, zTab, &nPk, 0);
		strInit(&ins);
		if (az == 0){
			pStmt = db_prepare(pDB, "SELECT * FROM %s.%s", zAux, zId);
			strPrintf(&ins, "INSERT INTO %s VALUES", zId);
		}
		else{
			Str sql;
			strInit(&sql);
			zSep = "SELECT";
			for (i = 0; az[i]; i++){
				strPrintf(&sql, "%s %s", zSep, az[i]);
				zSep = ",";
			}
			strPrintf(&sql, " FROM %s.%s", zAux, zId);
			zSep = " ORDER BY";
			for (i = 1; i <= nPk; i++){
				strPrintf(&sql, "%s %d", zSep, i);
				zSep = ",";
			}
			pStmt = db_prepare(pDB, "%s", sql.z);
			strFree(&sql);
			strPrintf(&ins, "INSERT INTO %s", zId);
			zSep = "(";
			for (i = 0; az[i]; i++){
				strPrintf(&ins, "%s%s", zSep, az[i]);
				zSep = ",";
			}
			strPrintf(&ins, ") VALUES");
			namelistFree(az);
		}
		nCol = sqlite3_column_count(pStmt);
		while (SQLITE_ROW == sqlite3_step(pStmt)){
			strPrintf(out, "%s", ins.z);
			zSep = "(";
			for (i = 0; i<nCol; i++){
				strPrintf(out, "%s", zSep);
				printQuoted(out, sqlite3_column_value(pStmt, i));
				zSep = ",";
			}
			strPrintf(out, ");\n");
		}
		sqlite3_finalize(pStmt);
		strFree(&ins);
//	} /* endif !g.bSchemaOnly */
		pStmt = db_prepare(pDB, "SELECT sql FROM %Q.sqlite_master"
		" WHERE type='index' AND tbl_name=%Q AND sql IS NOT NULL",
		zAux, zTab);
	while (SQLITE_ROW == sqlite3_step(pStmt)){
		strPrintf(out, "%s;\n", sqlite3_column_text(pStmt, 0));
	}
	sqlite3_finalize(pStmt);
}


/*
** Compute all differences for a single table.
*/
void diff_one_table(sqlite3 * pDB, const char *zMain, const char *zAux, const char *zTab, Str *out)
{
	char *zId = safeId(zTab); /* Name of table (translated for us in SQL) */
	char **az = 0;            /* Columns in main */
	char **az2 = 0;           /* Columns in aux */
	int nPk;                  /* Primary key columns in main */
	int nPk2;                 /* Primary key columns in aux */
	int n = 0;                /* Number of columns in main */
	int n2;                   /* Number of columns in aux */
	int nQ;                   /* Number of output columns in the diff query */
	int i;                    /* Loop counter */
	const char *zSep;         /* Separator string */
	Str sql;                  /* Comparison query */
	sqlite3_stmt *pStmt;      /* Query statement to do the diff */

	strInit(&sql);

	if (sqlite3_table_column_metadata(pDB, zAux, zTab, 0, 0, 0, 0, 0, 0)){
		if (!sqlite3_table_column_metadata(pDB, zMain, zTab, 0, 0, 0, 0, 0, 0)){
			/* Table missing from second database. */
			strPrintf(out, "DROP TABLE %s;\n", zId);
		}
		goto end_diff_one_table;
	}

	if (sqlite3_table_column_metadata(pDB, zMain, zTab, 0, 0, 0, 0, 0, 0)){
		/* Table missing from source */
		dump_table(pDB, zAux, zTab, out);
		goto end_diff_one_table;
	}

	az = columnNames(pDB, zMain, zTab, &nPk, 0);
	az2 = columnNames(pDB, zAux, zTab, &nPk2, 0);
	if (az && az2){
		for (n = 0; az[n] && az2[n]; n++){
			if (sqlite3_stricmp(az[n], az2[n]) != 0) break;
		}
	}
	if (az == 0
		|| az2 == 0
		|| nPk != nPk2
		|| az[n]
		){
		/* Schema mismatch */
		strPrintf(out, "DROP TABLE %s; -- due to schema mismatch\n", zId);
		dump_table(pDB, zAux, zTab, out);
		goto end_diff_one_table;
	}

	/* Build the comparison query */
	for (n2 = n; az2[n2]; n2++){
		strPrintf(out, "ALTER TABLE %s ADD COLUMN %s;\n", zId, safeId(az2[n2]));
	}
	nQ = nPk2 + 1 + 2 * (n2 - nPk2);
	if (n2>nPk2){
		zSep = "SELECT ";
		for (i = 0; i<nPk; i++){
			strPrintf(&sql, "%sB.%s", zSep, az[i]);
			zSep = ", ";
		}
		strPrintf(&sql, ", 1%s -- changed row\n", nPk == n ? "" : ",");
		while (az[i]){
			strPrintf(&sql, "       A.%s IS NOT B.%s, B.%s%s\n",
				az[i], az2[i], az2[i], az2[i + 1] == 0 ? "" : ",");
			i++;
		}
		while (az2[i]){
			strPrintf(&sql, "       B.%s IS NOT NULL, B.%s%s\n",
				az2[i], az2[i], az2[i + 1] == 0 ? "" : ",");
			i++;
		}
		strPrintf(&sql, "  FROM %s.%s A, %s.%s B\n", zMain, zId, zAux, zId);
		zSep = " WHERE";
		for (i = 0; i<nPk; i++){
			strPrintf(&sql, "%s A.%s=B.%s", zSep, az[i], az[i]);
			zSep = " AND";
		}
		zSep = "\n   AND (";
		while (az[i]){
			strPrintf(&sql, "%sA.%s IS NOT B.%s%s\n",
				zSep, az[i], az2[i], az2[i + 1] == 0 ? ")" : "");
			zSep = "        OR ";
			i++;
		}
		while (az2[i]){
			strPrintf(&sql, "%sB.%s IS NOT NULL%s\n",
				zSep, az2[i], az2[i + 1] == 0 ? ")" : "");
			zSep = "        OR ";
			i++;
		}
		strPrintf(&sql, " UNION ALL\n");
	}
	zSep = "SELECT ";
	for (i = 0; i<nPk; i++){
		strPrintf(&sql, "%sA.%s", zSep, az[i]);
		zSep = ", ";
	}
	strPrintf(&sql, ", 2%s -- deleted row\n", nPk == n ? "" : ",");
	while (az2[i]){
		strPrintf(&sql, "       NULL, NULL%s\n", i == n2 - 1 ? "" : ",");
		i++;
	}
	strPrintf(&sql, "  FROM %s.%s A\n", zMain, zId);
	strPrintf(&sql, " WHERE NOT EXISTS(SELECT 1 FROM %s.%s B\n", zAux, zId);
	zSep = "                   WHERE";
	for (i = 0; i<nPk; i++){
		strPrintf(&sql, "%s A.%s=B.%s", zSep, az[i], az[i]);
		zSep = " AND";
	}
	strPrintf(&sql, ")\n");
	zSep = " UNION ALL\nSELECT ";
	for (i = 0; i<nPk; i++){
		strPrintf(&sql, "%sB.%s", zSep, az[i]);
		zSep = ", ";
	}
	strPrintf(&sql, ", 3%s -- inserted row\n", nPk == n ? "" : ",");
	while (az2[i]){
		strPrintf(&sql, "       1, B.%s%s\n", az2[i], az2[i + 1] == 0 ? "" : ",");
		i++;
	}
	strPrintf(&sql, "  FROM %s.%s B\n", zAux, zId);
	strPrintf(&sql, " WHERE NOT EXISTS(SELECT 1 FROM %s.%s A\n", zMain, zId);
	zSep = "                   WHERE";
	for (i = 0; i<nPk; i++){
		strPrintf(&sql, "%s A.%s=B.%s", zSep, az[i], az[i]);
		zSep = " AND";
	}
	strPrintf(&sql, ")\n ORDER BY");
	zSep = " ";
	for (i = 1; i <= nPk; i++){
		strPrintf(&sql, "%s%d", zSep, i);
		zSep = ", ";
	}
	strPrintf(&sql, ";\n");

	//if (g.fDebug & DEBUG_DIFF_SQL){
	//	printf("SQL for %s:\n%s\n", zId, sql.z);
	//	goto end_diff_one_table;
	//}

	/* Drop indexes that are missing in the destination */
	pStmt = db_prepare(pDB,
		"SELECT name FROM %Q.sqlite_master"
		" WHERE type='index' AND tbl_name=%Q"
		"   AND sql IS NOT NULL"
		"   AND sql NOT IN (SELECT sql FROM %Q.sqlite_master"
		"                    WHERE type='index' AND tbl_name=%Q"
		"                      AND sql IS NOT NULL)",
		zMain, zTab, zAux, zTab);
	while (SQLITE_ROW == sqlite3_step(pStmt)){
		char *z = safeId((const char*)sqlite3_column_text(pStmt, 0));
		strPrintf(out, "DROP INDEX %s;\n", z);
		sqlite3_free(z);
	}
	sqlite3_finalize(pStmt);

	/* Run the query and output differences */
	//if (!g.bSchemaOnly){
		pStmt = db_prepare(pDB, sql.z);
		while (SQLITE_ROW == sqlite3_step(pStmt)){
			int iType = sqlite3_column_int(pStmt, nPk);
			if (iType == 1 || iType == 2){
				if (iType == 1){       /* Change the content of a row */
					strPrintf(out, "UPDATE %s", zId);
					zSep = " SET";
					for (i = nPk + 1; i<nQ; i += 2){
						if (sqlite3_column_int(pStmt, i) == 0) continue;
						strPrintf(out, "%s %s=", zSep, az2[(i + nPk - 1) / 2]);
						zSep = ",";
						printQuoted(out, sqlite3_column_value(pStmt, i + 1));
					}
				}
				else{                /* Delete a row */
					strPrintf(out, "DELETE FROM %s", zId);
				}
				zSep = " WHERE";
				for (i = 0; i<nPk; i++){
					strPrintf(out, "%s %s=", zSep, az2[i]);
					printQuoted(out, sqlite3_column_value(pStmt, i));
					zSep = " AND";
				}
				strPrintf(out, ";\n");
			}
			else{                  /* Insert a row */
				strPrintf(out, "INSERT INTO %s(%s", zId, az2[0]);
				for (i = 1; az2[i]; i++) strPrintf(out, ",%s", az2[i]);
				strPrintf(out, ") VALUES");
				zSep = "(";
				for (i = 0; i<nPk2; i++){
					strPrintf(out, "%s", zSep);
					zSep = ",";
					printQuoted(out, sqlite3_column_value(pStmt, i));
				}
				for (i = nPk2 + 2; i<nQ; i += 2){
					strPrintf(out, ",");
					printQuoted(out, sqlite3_column_value(pStmt, i));
				}
				strPrintf(out, ");\n");
			}
		}
		sqlite3_finalize(pStmt);
	//} /* endif !g.bSchemaOnly */

	/* Create indexes that are missing in the source */
		pStmt = db_prepare(pDB,
		"SELECT sql FROM %Q.sqlite_master"
		" WHERE type='index' AND tbl_name=%Q"
		"   AND sql IS NOT NULL"
		"   AND sql NOT IN (SELECT sql FROM %Q.sqlite_master"
		"                    WHERE type='index' AND tbl_name=%Q"
		"                      AND sql IS NOT NULL)",
		zAux, zTab, zMain, zTab);
	while (SQLITE_ROW == sqlite3_step(pStmt)){
		strPrintf(out, "%s;\n", sqlite3_column_text(pStmt, 0));
	}
	sqlite3_finalize(pStmt);

end_diff_one_table:
	strFree(&sql);
	sqlite3_free(zId);
	namelistFree(az);
	namelistFree(az2);
	return;
}
