#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../core/sqlite3.h"
#include "assert.h"
#include <time.h>

#include <unistd.h>  /* for STDERR_FILENO */
#include "../common/backtrace.c"

//#define QUIT_TEST()  print_backtrace()
#define QUIT_TEST()  exit(1)

int bind_sql_parameters(sqlite3_stmt *stmt, const char *types, va_list ap);

/****************************************************************************/

void print_error(int rc, char *desc, char *sql, const char *function, int line){

  printf("\n\tFAIL %d: %s\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", rc, desc, sql, function, line);

}

/****************************************************************************/

void db_execute_fn(sqlite3 *db, char *sql, const char *function, int line){
  char *zErrMsg=0;
  int rc;

  rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
  if( rc!=SQLITE_OK ){
    print_error(rc, zErrMsg, sql, function, line);
    sqlite3_free(zErrMsg);
    QUIT_TEST();
  }

}

#define db_execute(db,sql) db_execute_fn(db, sql, __FUNCTION__, __LINE__)

/****************************************************************************/

void db_catch_fn(sqlite3 *db, char *sql, const char *function, int line){
  char *zErrMsg=0;
  int rc;

  rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
  if( rc==SQLITE_OK ){
    print_error(rc, "expected error was not generated", sql, function, line);
    sqlite3_free(zErrMsg);
    QUIT_TEST();
  }

}

#define db_catch(db,sql) db_catch_fn(db, sql, __FUNCTION__, __LINE__)

/****************************************************************************/

void db_execute_many_fn(sqlite3 *db, char *sql, char *types, int count,
  const char *function, int line, ...
){
  va_list args;
  sqlite3_stmt *stmt=0;
  char *zErrMsg=0;
  int rc, i;

  db_execute_fn(db, "BEGIN", function, line);

  rc = sqlite3_prepare(db, sql, -1, &stmt, NULL);
  if( rc!=SQLITE_OK ){
    print_error(rc, zErrMsg, sql, function, line);
    sqlite3_free(zErrMsg);
    QUIT_TEST();
  }

  va_start(args, line);

  for( i=0; i<count; i++ ){
    rc = bind_sql_parameters(stmt, types, args);
    if( rc!=SQLITE_OK ){
      print_error(rc, "bind parameters", sql, function, line);
      QUIT_TEST();
    }

    rc = sqlite3_step(stmt);
    if( rc!=SQLITE_DONE ){
      print_error(rc, "sqlite3_step", sql, function, line);
      QUIT_TEST();
    }

    rc = sqlite3_reset(stmt);
    if( rc!=SQLITE_OK ){
      print_error(rc, "sqlite3_reset", sql, function, line);
      QUIT_TEST();
    }
  }

  va_end(args);

  sqlite3_finalize(stmt);

  db_execute_fn(db, "COMMIT", function, line);

}

#define db_execute_many(db,sql,types,count,...) db_execute_many_fn(db, sql, types, count, __FUNCTION__, __LINE__, __VA_ARGS__)

/****************************************************************************/

void db_check_int_fn(sqlite3 *db, char *sql, int expected, const char *function, int line){
  sqlite3_stmt *stmt=0;
  const char *zTail=0;
  int rc, returned;

  do{

    rc = sqlite3_prepare(db, sql, -1, &stmt, &zTail);
    if( rc!=SQLITE_OK ){
      print_error(rc, "sqlite3_prepare", sql, function, line);
      QUIT_TEST();
    }

    rc = sqlite3_step(stmt);

    if( zTail && zTail[0] ){

      if( rc!=SQLITE_DONE ){
        print_error(rc, "multi command returned a row", sql, function, line);
        QUIT_TEST();
      }

      sql = (char*) zTail;

    } else {

      if( rc!=SQLITE_ROW ){
        print_error(rc, "no row returned", sql, function, line);
        QUIT_TEST();
      }

      if( sqlite3_column_count(stmt)!=1 ){
        printf("\n\tFAIL: returned %d columns\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", sqlite3_column_count(stmt), sql, function, line);
        QUIT_TEST();
      }

      returned = sqlite3_column_int(stmt, 0);
      if( returned!=expected ){
        printf("\n\tFAIL: expected=%d returned=%d\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", expected, returned, sql, function, line);
        QUIT_TEST();
      }

      rc = sqlite3_step(stmt);
      if( rc!=SQLITE_DONE ){
        printf("\n\tFAIL: additional row returned\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", sql, function, line);
        QUIT_TEST();
      }

      sql = NULL;

    }

    sqlite3_finalize(stmt);

  } while( sql );

}

#define db_check_int(db,sql,expected) db_check_int_fn(db, sql, expected, __FUNCTION__, __LINE__)

/****************************************************************************/

void db_check_str_fn(sqlite3 *db, char *sql, char *expected, const char *function, int line){
  sqlite3_stmt *stmt=0;
  const char *zTail=0;
  char *returned;
  int rc;

  do{

    rc = sqlite3_prepare(db, sql, -1, &stmt, &zTail);
    if( rc!=SQLITE_OK ){
      print_error(rc, "sqlite3_prepare", sql, function, line);
      QUIT_TEST();
    }

    rc = sqlite3_step(stmt);

    if( zTail && zTail[0] ){

      if( rc!=SQLITE_DONE ){
        print_error(rc, "multi command returned a row", sql, function, line);
        QUIT_TEST();
      }

      sql = (char*) zTail;

    } else {

      if( rc!=SQLITE_ROW ){
        print_error(rc, "no row returned", sql, function, line);
        QUIT_TEST();
      }

      if( sqlite3_column_count(stmt)!=1 ){
        printf("\n\tFAIL: returned %d columns\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", sqlite3_column_count(stmt), sql, function, line);
        QUIT_TEST();
      }

      returned = (char*) sqlite3_column_text(stmt, 0);
      if( strcmp(returned,expected)!=0 ){
        printf("\n\tFAIL: expected='%s' returned='%s'\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", expected, returned, sql, function, line);
        QUIT_TEST();
      }

      rc = sqlite3_step(stmt);
      if( rc!=SQLITE_DONE ){
        printf("\n\tFAIL: additional row returned\n\tsql: %s\n\tfunction: %s\n\tline: %d\n", sql, function, line);
        QUIT_TEST();
      }

      sql = NULL;

    }

    sqlite3_finalize(stmt);

  } while( sql );

}

#define db_check_str(db,sql,expected) db_check_str_fn(db, sql, expected, __FUNCTION__, __LINE__)

/****************************************************************************/

void db_check_empty(sqlite3 *db, char *sql){
  sqlite3_stmt *stmt=0;
  const char *zTail=0;
  int rc;

  do{

    rc = sqlite3_prepare(db, sql, -1, &stmt, &zTail);
    if( rc!=SQLITE_OK ){
      printf("FAIL %d: prepare\n\tsql: %s\n", rc, sql);
      QUIT_TEST();
    }

    rc = sqlite3_step(stmt);

    if( zTail && zTail[0] ){

      if( rc!=SQLITE_DONE ){
        printf("FAIL %d: multi command returned a row\nsql: %s\n", rc, sql);
        QUIT_TEST();
      }

      sql = (char*) zTail;

    } else {

      if( rc==SQLITE_ROW ){
        printf("FAIL: unexpected row returned\nsql: %s\n", sql);
        QUIT_TEST();
      }

      sql = NULL;

    }

    sqlite3_finalize(stmt);

  } while( sql );

}

/****************************************************************************/
/****************************************************************************/

int bind_sql_parameters(sqlite3_stmt *stmt, const char *types, va_list ap){
  int rc, parameter_count, iDest, i;
  char c;

  parameter_count = sqlite3_bind_parameter_count(stmt);

  iDest = 1;
  for(i=0; (c = types[i])!=0; i++){
    if( i+1>parameter_count ) goto loc_invalid;
    if( c=='s' ){
      char *z = va_arg(ap, char*);
      sqlite3_bind_text(stmt, iDest+i, z, -1, SQLITE_TRANSIENT);
    }else if( c=='i' ){
      sqlite3_bind_int64(stmt, iDest+i, va_arg(ap, int));
    }else if( c=='l' ){
      sqlite3_bind_int64(stmt, iDest+i, va_arg(ap, int64));
    //}else if( c=='f' ){  -- floats are promoted to double
    //  sqlite3_bind_double(stmt, iDest+i, va_arg(ap, float));
    }else if( c=='d' ){
      sqlite3_bind_double(stmt, iDest+i, va_arg(ap, double));
    }else if( c=='b' ){
      char *ptr = va_arg(ap, char*);
      int size = va_arg(ap, int);
      sqlite3_bind_blob(stmt, iDest+i, ptr, size, SQLITE_TRANSIENT);
      iDest++;
    }else{
      goto loc_invalid;
    }
  }

  return SQLITE_OK;

loc_invalid:
  return SQLITE_MISUSE;
}

/****************************************************************************/

int db_prepare_and_bind(sqlite3 *db, const char *sql, va_list args, sqlite3_stmt **pstmt){
  sqlite3_stmt *stmt=0;
  int rc;

  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if( rc!=SQLITE_OK ) goto loc_exit;
  if( !stmt ) goto loc_failed;

  if( sqlite3_bind_parameter_count(stmt)>0 ){
    const char *types;
    types = va_arg(args, char*);
    rc = bind_sql_parameters(stmt, types, args);
    if( rc ){
      sqlite3_finalize(stmt);
      goto loc_exit;
    }
  }

  *pstmt = stmt;

loc_exit:
  return rc;

loc_failed:
  rc = SQLITE_ERROR;
  goto loc_exit;

}

/****************************************************************************/

int db_exec_bindv(sqlite3 *db, const char *sql, va_list args){
  sqlite3_stmt *stmt=NULL;
  int rc;

  rc = db_prepare_and_bind(db, sql, args, &stmt);
  if( rc ) goto loc_exit;

  rc = sqlite3_step(stmt);
  if( rc==SQLITE_DONE ) rc = SQLITE_OK;

loc_exit:
  sqlite3_finalize(stmt);
  return rc;
}

/****************************************************************************/

int db_exec(sqlite3 *db, const char *sql, ...){
  va_list args;
  int rc;

  va_start(args, sql);
  rc = db_exec_bindv(db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

int db_query(
  sqlite3 *db,
  int(*callback)(void*,sqlite3_stmt*),
  void *user,
  const char *sql,
  ...
){
  sqlite3_stmt *stmt = 0;
  va_list args;
  int rc;

  if( !sql ) sql = "";

  va_start(args, sql);
  rc = db_prepare_and_bind(db, sql, args, &stmt);
  va_end(args);

  if( rc ) goto loc_exit;

  while( (rc=sqlite3_step(stmt))==SQLITE_ROW ){
    /* call the callback function */
    rc = callback(user, stmt);
    if( rc ) goto loc_exit;
  }

  if( rc==SQLITE_DONE ) rc = SQLITE_OK;

loc_exit:
  if( stmt ) sqlite3_finalize(stmt);
  return rc;

}

/****************************************************************************/

#ifndef SQLITE_INT32
#define SQLITE_INT32 12732
#endif
#ifndef SQLITE_INT64
#define SQLITE_INT64 12764
#endif

struct blob_info {
  char *ptr;
  int size;
};

int db_query_value_stmt(void **pvalue, int type, sqlite3_stmt *stmt) {
  int rc, ncols, nrows=0;

  ncols = sqlite3_column_count(stmt);
  if( ncols!=1 ) goto loc_failed;

  while( (rc=sqlite3_step(stmt))==SQLITE_ROW ){
    nrows++;
    if( nrows>1 ) goto loc_failed;
    //
    //switch (sqlite3_column_type(stmt, 0)) {
    switch( type ){
      case SQLITE_INT32:
        *(int*)pvalue = sqlite3_column_int(stmt, 0);
        break;
      case SQLITE_INT64:
        *(int64*)pvalue = sqlite3_column_int64(stmt, 0);
        break;
      case SQLITE_FLOAT:
        *(double*)pvalue = sqlite3_column_double(stmt, 0);
        break;
      case SQLITE_TEXT: {
        char *text = (char*)sqlite3_column_text(stmt, 0);
        text = sqlite3_strdup(text);
        *(char**)pvalue = text;
        if( !text ){ rc = SQLITE_NOMEM; goto loc_exit; }
        break;
      }
      case SQLITE_BLOB: {
        struct blob_info *blob = (struct blob_info *)pvalue;
        blob->ptr = (char*)sqlite3_column_blob(stmt, 0);
        blob->size = sqlite3_column_bytes(stmt, 0);
        blob->ptr = sqlite3_memdup(blob->ptr, blob->size);
        if( !blob->ptr ){ rc = SQLITE_NOMEM; goto loc_exit; }
        break;
      }
      default:
        goto loc_failed;
    }
  }

  if( rc==SQLITE_DONE ) rc = SQLITE_OK;

loc_exit:
  if( stmt ) sqlite3_finalize(stmt);
  return rc;

loc_failed:
  rc = SQLITE_ERROR;
  goto loc_exit;

}

/****************************************************************************/

int db_query_valuev(void **pvalue, int type, sqlite3 *db, const char *sql, va_list args) {
  sqlite3_stmt *stmt = 0;
  int rc;

  if( !sql ) sql = "";

  rc = db_prepare_and_bind(db, sql, args, &stmt);
  if( rc ) goto loc_exit;

  rc = db_query_value_stmt(pvalue, type, stmt);

loc_exit:
  return rc;

loc_failed:
  rc = SQLITE_ERROR;
  goto loc_exit;

}

/****************************************************************************/

int db_query_value(void **pvalue, int type, sqlite3 *db, const char *sql, ...) {
  va_list args;
  int rc;

  va_start(args, sql);
  rc = db_query_valuev(pvalue, type, db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

int db_query_int64(int64 *pvalue, sqlite3 *db, char *sql, ...) {
  va_list args;
  int rc;

  if ( !pvalue ) return SQLITE_MISUSE;
  *pvalue = 0;

  va_start(args, sql);
  rc = db_query_valuev((void**)pvalue, SQLITE_INT64, db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

int db_query_int32(int *pvalue, sqlite3 *db, char *sql, ...) {
  va_list args;
  int rc;

  if ( !pvalue ) return SQLITE_MISUSE;
  *pvalue = 0;

  va_start(args, sql);
  rc = db_query_valuev((void**)pvalue, SQLITE_INT32, db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

int db_query_double(double *pvalue, sqlite3 *db, char *sql, ...) {
  va_list args;
  int rc;

  if ( !pvalue ) return SQLITE_MISUSE;
  *pvalue = 0;

  va_start(args, sql);
  rc = db_query_valuev((void**)pvalue, SQLITE_FLOAT, db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

/* returned memory must be released with sqlite3_free */
int db_query_str(char **pvalue, sqlite3 *db, char *sql, ...) {
  va_list args;
  int rc;

  if ( !pvalue ) return SQLITE_MISUSE;
  *pvalue = NULL;

  va_start(args, sql);
  rc = db_query_valuev((void**)pvalue, SQLITE_TEXT, db, sql, args);
  va_end(args);

  return rc;
}

/****************************************************************************/

/* returned memory must be released with sqlite3_free */
int db_query_blob(char **pvalue, int *psize, sqlite3 *db, char *sql, ...) {
  struct blob_info blob = {0};
  va_list args;
  int rc;

  if ( !pvalue ) return SQLITE_MISUSE;
  *pvalue = NULL;

  va_start(args, sql);
  rc = db_query_valuev((void**)&blob, SQLITE_BLOB, db, sql, args);
  va_end(args);

  *pvalue = blob.ptr;
  if( psize ) *psize = blob.size;

  return rc;
}

/****************************************************************************/
/****************************************************************************/

void unlinkf(char *base, ...){
  char name[128];
  va_list ap;
  va_start(ap, base);
  vsnprintf(name, 128, base, ap);
  va_end(ap);
  unlink(name);
}

/****************************************************************************/

int random_number(int lower, int upper){
  static bool init = false;
  if( !init ){
    srand(time(0));
    init = true;
  }
  return (rand() % (upper - lower + 1)) + lower;
}

/****************************************************************************/

bool in_array(int item, int len, int list[]){
  for(int i=0; i<len; i++){
    if( list[i]==item ) return true;
  }
  return false;
}

/****************************************************************************/

bool in_array_list(int item, int list[]){
  for(int i=0; list[i]; i++){
    if( list[i]==item ) return true;
  }
  return false;
}

/****************************************************************************/

int len_array_list(int list[]){
  int count = 0;
  for(int i=0; list[i]; i++) count++;
  return count;
}

/****************************************************************************/

void add_to_array_list(int list[], int item){
  int i;
  for(i=0; list[i]; i++){}
  list[i] = item;
}

/****************************************************************************/

void clear_array_list(int list[]){
  for(int i=0; list[i]; i++){
    list[i] = 0;
  }
}
