#include <stdbool.h>

#ifdef SQLITE_DEBUG
#define XTRACE(...)   printf(__VA_ARGS__)
#else
#define XTRACE(...)
#endif

#define CMD_TYPE_DECLARE    1
#define CMD_TYPE_SET        2
#define CMD_TYPE_STATEMENT  3
#define CMD_TYPE_RETURN     4
#define CMD_TYPE_RAISE      5
#define CMD_TYPE_ASSERT     6

#define CMD_TYPE_IF         7
#define CMD_TYPE_ELSEIF     8
#define CMD_TYPE_ELSE       9
#define CMD_TYPE_ENDIF      10

#define CMD_TYPE_LOOP       11
#define CMD_TYPE_ENDLOOP    12
#define CMD_TYPE_BREAK      13
#define CMD_TYPE_CONTINUE   14
#define CMD_TYPE_FOREACH    15


#define CMD_FLAG_STORE_AS_ARRAY  1
#define CMD_FLAG_DYNAMIC_SQL     2
#define CMD_FLAG_EXECUTED        4


#define POS_RESULT_ROW      2
#define POS_NEXT_RESULT     3


typedef struct sqlite3_var sqlite3_var;

struct sqlite3_var {
  char name[32];          /* variable name, including the @ */
  int len;                /* variable name size */
  u8 type;  //affinity;   /* defined type. SQLITE_INTEGER, REAL, TEXT or BLOB */
  int declared_in_pos;    /* position in the procedure where it was declared */
  sqlite3_value value;    /* contains a value or a pointer to an array struct */
  sqlite3_var *next;      /* next in the global list */
  sqlite3_var *nextUsed;  /* temporary use */
};

#define VAR_POS_PARAMETER  -2

// when a sqlite3_var contains an array, the sqlite3_value has a pointer to a sqlite3_array structure

typedef struct sqlite3_array sqlite3_array;

struct sqlite3_array {
    int num_items;
    sqlite3_value value[1];
};

typedef struct stored_proc stored_proc;
typedef struct command command;

struct command {
    int type;
    char *sql, *sql2;
    int  nsql, nsql2;
    sqlite3_stmt  *stmt;        /* used in STATEMENT, SET, FOREACH and RETURN */
    sqlite3_array *input_array; /* parsed ARRAY, used in SET, FOREACH and CALL commands */
    sqlite3_var   *input_var;   /* used in the FOREACH command */
    unsigned int current_item;  /* used in the FOREACH command */

    int flags;

    stored_proc *procedure;     /* used in CALL command */

    int next_if_cmd;            /* used in ELSEIF, ELSE, END IF */
    int related_cmd;            /* used in LOOP, BREAK, CONTINUE, END LOOP, FOREACH */

    sqlite3_var **vars;         /* variables used in this command (array of pointers to) */
    unsigned int num_vars;
};

struct stored_proc {
    sqlite3 *db;
    char name[128];
    bool is_function;
    char *code;
    char *error_msg;
    // commands
    command* cmds;                  // an array of commands (pointer to allocated memory)
    unsigned int num_alloc_cmds;    // the current size of the array (number of elements)
    unsigned int num_cmds;
    // variables
    sqlite3_var* vars;
    //unsigned int num_vars;
    // parameters = variables declared in the procedure header
    sqlite3_var** params;  // an array of pointers to variables
    unsigned int num_params;
    // result
    sqlite3_array *result_array;
    int current_row;
    // aMem and nMem from Vdbe are temporarily stored here
    sqlite3_value *aMem;
    int nMem;
};


struct procedure_call {
    stored_proc *procedure;
    sqlite3_array *input_array;
};


////////////////////////////////////////////////////////////////////////////////

SQLITE_PRIVATE sqlite3_var* findVariable(stored_proc *procedure, char *name, int len);

SQLITE_PRIVATE int parse_variables_list(
  stored_proc* procedure,
  int cmd_pos,
  char** psql,
  unsigned int *pnum_vars,
  sqlite3_var **pvar_list
);

SQLITE_PRIVATE int parse_input_array(Parse *pParse, stored_proc* procedure, int cmd_pos, char** psql);

SQLITE_PRIVATE int parse_procedure_body(Parse *pParse, stored_proc* procedure, char** psql);

SQLITE_PRIVATE void releaseProcedure(stored_proc* procedure);
SQLITE_PRIVATE void releaseProcedureCall(procedure_call* call);

////////////////////////////////////////////////////////////////////////////////

/*
** Identifies the next `;` token, and returns the position of the next token.
** If the `;` is inside a string or identifier, it will be ignored.
** If the `;` is inside a comment, it will be ignored.
** Use the sqlite3GetToken() function to identify the tokens
*/
SQLITE_PRIVATE int skip_sql_command(char **psql){
  char *sql = *psql;
  int n;
  int token_type = 0;

  while( (n = sqlite3GetToken((u8*)sql, &token_type)) != 0 ){
    if( token_type == TK_SEMI ){
      break;
    }
    sql += n;
  }

  n = sql - *psql;
  if( token_type == TK_SEMI ){
    sql++;
  }
  *psql = sql;
  return n;
}

SQLITE_PRIVATE int skip_delimited_sql_command(char **psql, int delimiter, int ndelim){
  char *sql = *psql;
  int n;
  int token_type = 0;
  bool found = false;

  while( (n = sqlite3GetToken((u8*)sql, &token_type)) != 0 ){
    if( token_type == delimiter ){
      found = true;
      break;
    }
    sql += n;
  }

  // if the delimiter was not found, return -1
  if( !found ) return -1;

  // get the size of the command
  n = sql - *psql;

  // skip the delimiter
  sql += ndelim;

  // skip whitespaces
  while( sqlite3Isspace(*sql) ) sql++;

  // return the position of the next token  
  *psql = sql;

  // return the size of the command
  return n;
}

////////////////////////////////////////////////////////////////////////////////
// VALUES
////////////////////////////////////////////////////////////////////////////////

SQLITE_PRIVATE void sqlite3ValueSetSubtype(sqlite3_value *pVal, unsigned int eSubtype){
  pVal->eSubtype = eSubtype & 0xff;
  pVal->flags |= MEM_Subtype;
}

/*
** Store a variable name in a value.
*/
SQLITE_PRIVATE void sqlite3ValueSetVariable(
  sqlite3_value *value, char *name, int len, u8 enc
){
  sqlite3ValueSetStr(value, len, name, enc, SQLITE_TRANSIENT);
  sqlite3ValueSetSubtype(value, 'v');
}

/*
** Store a pointer to an array in a value.
*/
SQLITE_PRIVATE void sqlite3ValueSetArray(
  sqlite3_value *value, sqlite3_array *array, void (*free_func)(sqlite3_array*)
){
  sqlite3VdbeMemSetNull(value);
  sqlite3VdbeMemSetPointer(value, array, "array", (void(*)(void*))free_func);
}

/*
** Return true if the supplied value contains a variable.
*/
SQLITE_PRIVATE bool is_variable(sqlite3_value *pVal){
  return (sqlite3_value_type(pVal) == SQLITE_TEXT) && (sqlite3_value_subtype(pVal) == 'v');
}

/*
** Return true if the value contains an array.
*/
SQLITE_PRIVATE bool is_array(sqlite3_value *pVal){
  return sqlite3_value_pointer(pVal, "array") != NULL;
}

/*
** Return the array stored in the value.
*/
SQLITE_PRIVATE sqlite3_array* get_array_from_value(sqlite3_value *pVal){
  return sqlite3_value_pointer(pVal, "array");
}

/*
** Fill the sqlite3_value object with the value of the given token.
**
** This only works for very simple expressions that consist of one constant
** token (i.e. "12", "-3.4", "'a string'", "TRUE", "FALSE", "NULL", "x'0102'").
** If the expression cannot be converted to a value, SQLITE_ERROR is returned.
*/
SQLITE_PRIVATE int sqlite3ValueFromToken(
  char **pzToken,           /* The token to evaluate */
  int nToken,  
  int tk,
  u8 enc,                   /* Encoding to use */
  sqlite3_value *pVal       /* Write the new value here */
){
  char *zToken = *pzToken;
  int rc = SQLITE_OK;

  assert( pVal!=NULL );

  if( tk==TK_MINUS ){
    assert( nToken==1 );
    // get the next token
    zToken++;
    nToken = sqlite3GetToken((u8*)zToken, &tk);
    // if the next token is not an integer or float, then it is not a negative integer
    if( tk!=TK_INTEGER && tk!=TK_FLOAT ){
      return SQLITE_ERROR;
    }
    zToken--;
    nToken++;
  }

  if( tk==TK_INTEGER ){
    i64 value = 0;
    sqlite3Atoi64(zToken, &value, nToken, enc);
    sqlite3VdbeMemSetInt64(pVal, value);
  }else if( tk==TK_FLOAT ){
    double value = (double) 0;
    sqlite3AtoF(zToken, &value, nToken, enc);
    sqlite3VdbeMemSetDouble(pVal, value);
  }else if( tk==TK_STRING ){
    if( nToken>0 && sqlite3Isquote(zToken[0]) ){
      char *zStr = sqlite3StrNDup(zToken, nToken);
      sqlite3Dequote(zStr);
      sqlite3VdbeMemSetStr(pVal, zStr, -1, enc, SQLITE_DYNAMIC);
    } else if( nToken>0 ){
      sqlite3VdbeMemSetStr(pVal, zToken, nToken, enc, SQLITE_TRANSIENT);
    }else{
      sqlite3VdbeMemSetStr(pVal, "", 0, enc, SQLITE_STATIC);
    }
  }
#ifndef SQLITE_OMIT_BLOB_LITERAL
  else if( tk==TK_BLOB ){
    char *zVal;
    int nVal;
    assert( zToken[0]=='x' || zToken[0]=='X' );
    assert( zToken[1]=='\'' );
    zVal = &zToken[2];
    nVal = nToken - 3;
    assert( zVal[nVal]=='\'' );
    zVal = sqlite3HexToBlob2(zVal, nVal);
    if( zVal==NULL ) return SQLITE_NOMEM;
    sqlite3VdbeMemSetStr(pVal, zVal, nVal/2, 0, SQLITE_DYNAMIC);
  }
#endif
  else if( tk==TK_ID ){
    if( nToken==4 && sqlite3_strnicmp(zToken, "true", nToken)==0 ){
      pVal->flags = MEM_Int;
      pVal->u.i = 1;
    } else if( nToken==5 && sqlite3_strnicmp(zToken, "false", nToken)==0 ){
      pVal->flags = MEM_Int;
      pVal->u.i = 0;
    } else if( nToken==4 && sqlite3_strnicmp(zToken, "null", nToken)==0 ){
      sqlite3VdbeMemSetNull(pVal);
    } else {
      rc = SQLITE_ERROR;
    }
  } else {
    rc = SQLITE_ERROR;
  }

  if( rc==SQLITE_OK ){
    zToken += nToken;
    *pzToken = zToken;
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// ARRAYS
////////////////////////////////////////////////////////////////////////////////

/*
** Release the content of the array, recursively.
** If the value contains a sub-array, then the sub-array is also released.
*/
SQLITE_PRIVATE void sqlite3_free_array(sqlite3_array *array) {
    if (array == NULL) return;
    for (int i = 0; i < array->num_items; i++) {
        // release the value
        // if it contains an array, it is released recursively
        sqlite3VdbeMemRelease(&array->value[i]);
    }
    sqlite3_free(array);
}

/*
** Parse an array
** It can contain internal arrays.
** The ARRAY keyword is optional.
** Examples:
**   ARRAY(1,2,3,ARRAY(4,5,6))
**   ARRAY(1,2,3,(4,5,6))
**   (1,2,3,(4,5,6))
*/
SQLITE_PRIVATE int parse_array(
  Parse *pParse, stored_proc* procedure, char** psql, sqlite3_array **parray
){
    char* sql = *psql;
    int rc = SQLITE_OK;
    int n, tokenType;
    sqlite3_array* array = NULL;

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // check for the optional "ARRAY" keyword
    if (sqlite3_strnicmp(sql, "ARRAY", 5) == 0) {
      sql += 5;
      while (sqlite3Isspace(*sql)) sql++;
    }

    // check for "("
    if (*sql != '(') return SQLITE_ERROR;
    sql++;
    while (sqlite3Isspace(*sql)) sql++;

    // parse the array values
    while (1) {
        if (array == NULL) {
            // allocate the array object with 1 item
            array = sqlite3MallocZero( sizeof(sqlite3_array) );
            if (!array) return SQLITE_NOMEM;
        } else {
            // increment the array size by 1 item
            sqlite3_array* new_array;
            new_array = sqlite3Realloc(array, sizeof(sqlite3_array) + 
                         (array->num_items) * sizeof(sqlite3_value));
            if (!new_array) {
              sqlite3_free_array(array);
              return SQLITE_NOMEM;
            }
            array = new_array;
            memset(&array->value[array->num_items], 0, sizeof(sqlite3_value));
        }
        // get a reference to the new value
        sqlite3_value *value = &array->value[array->num_items];

        // initialize the value
        sqlite3VdbeMemInit(value, pParse->db, MEM_Null);

        // get the next token
        n = sqlite3GetToken((u8*)sql, &tokenType);

        if (tokenType == TK_VARIABLE) {
            // if parsing a stored procedure, then the variable must exist
            if (procedure) {
                sqlite3_var *var;
                var = findVariable(procedure, sql, n);
                if (!var) {
                    sqlite3ErrorMsg(pParse, "variable must exist: %.*s", n, sql);
                    goto loc_invalid;
                }
            }
            // save the variable name into the value
            sqlite3ValueSetVariable(value, sql, n, SQLITE_UTF8);
            // skip the variable name
            sql += n;
        } else if (tokenType == TK_LP || (tokenType == TK_ID &&
                  n == 5 && sqlite3_strnicmp(sql, "ARRAY", 5) == 0)) {
            // parse the internal array
            sqlite3_array *internal_array;
            rc = parse_array(pParse, procedure, &sql, &internal_array);
            if (rc != SQLITE_OK) {
                goto loc_invalid;
            }
            // store the pointer to the internal array on the value
            sqlite3ValueSetArray(value, internal_array, sqlite3_free_array);
        } else if (tokenType == TK_RP) {
            // this is an empty array
            sql++;
            break;
        } else {
            // retrieve the value from the token
            rc = sqlite3ValueFromToken(&sql, n, tokenType, SQLITE_UTF8, value);
#ifdef SQLITE_DEBUG
            printf("parse_array() pos=%d value=", array->num_items);
            memTracePrint(value);
            puts("");
#endif
            if (rc != SQLITE_OK) {
                goto loc_invalid;
            }
        }

        // we have a new value
        array->num_items++;

        // skip whitespaces
        while (sqlite3Isspace(*sql)) sql++;

        // check for "," or ")"
        if (*sql == ',') {
            sql++;
            while (sqlite3Isspace(*sql)) sql++;
        } else if (*sql == ')') {
            sql++;
            break;
        } else {
            goto loc_invalid;
        }
    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    *psql = sql;
    *parray = array;
    return SQLITE_OK;

loc_invalid:
    if (array) sqlite3_free_array(array);
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    if (pParse->zErrMsg == NULL) {
      sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
    }
    *psql = sql;
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// VARIABLES
////////////////////////////////////////////////////////////////////////////////

/*
** Add a new local variable to the list or return the existing
** one with the supplied name.
*/
SQLITE_PRIVATE sqlite3_var* addVariable(
  stored_proc *procedure, char *name, int len, u8 type, bool *pExists
){
  sqlite3_var *var;

  if( pExists ) *pExists = false;

  /* check the variable name size */
  if( len>sizeof(var->name)-1 ){
    procedure->error_msg = sqlite3_mprintf("variable name must be up to 31 bytes long: %.*s", len, name);
    return NULL;
  }

  /* check if already exists */
  for( var=procedure->vars; var; var=var->next ){
    /* use case sensitive variables */
    if( var->len==len && sqlite3_strnicmp(var->name, name, len)==0 ){
      if( pExists ) *pExists = true;
      return var;
    }
  }

  var = sqlite3MallocZero(sizeof(struct sqlite3_var));
  if( !var ){
    procedure->error_msg = sqlite3_mprintf("out of memory");
    return NULL;
  }

  strncpy(var->name, name, len);
  var->len = len;
  var->type = type;

  /* initialize the value */
  sqlite3VdbeMemInit(&var->value, procedure->db, MEM_Null);

  /* add to the list of variables */
  var->next = procedure->vars;
  procedure->vars = var;

  return var;
}

/*
** Find a local variable with the supplied name.
*/
SQLITE_PRIVATE sqlite3_var* findVariable(stored_proc *procedure, char *name, int len){
  sqlite3_var *var;

  /* check the variable name size */
  if( len>sizeof(var->name)-1 ){
    procedure->error_msg = sqlite3_mprintf("variable name must be up to 31 bytes long: %.*s", len, name);
    return NULL;
  }

  /* check if already exists */
  for( var=procedure->vars; var; var=var->next ){
    /* use case sensitive variables */
    if( var->len==len && sqlite3_strnicmp(var->name, name, len)==0 ){
      return var;
    }
  }

  return NULL;
}

/*
** Drop all local variables.
*/
SQLITE_PRIVATE void dropAllVariables(stored_proc *procedure){

  assert( procedure!=NULL );

  /* clear the list of variables */
  while( procedure->vars ){
    sqlite3_var *current = procedure->vars;
    // if it is an array, free it
    //sqlite3_array *array = get_array_from_value(&current->value);
    //if( array ) sqlite3_free_array(array);
    // the free function may be called by sqlite3VdbeMemRelease
    // release the value
    sqlite3VdbeMemRelease((Mem*)&current->value);
    // release the variable
    sqlite3_var *next = current->next;
    sqlite3_free(current);
    procedure->vars = next;
  }

}

/*
** Bind values of local variables to the prepared statement.
*/
SQLITE_PRIVATE void bindLocalVariables(stored_proc *procedure, sqlite3_stmt *stmt){
  sqlite3_var *var;

  XTRACE("bindLocalVariables count=%d \n", sqlite3_bind_parameter_count(stmt));

  if( !procedure->vars || sqlite3_bind_parameter_count(stmt)==0 ) return;

  /* for each declared variable, check if being used in the statement */
  for( var=procedure->vars; var; var=var->next ){
    int idx = sqlite3_bind_parameter_index(stmt, var->name);
    XTRACE("bindLocalVariables %s idx=%d \n", var->name, idx);
    if( idx>0 ){
      sqlite3_bind_value(stmt, idx, &var->value);
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
// COMMANDS
////////////////////////////////////////////////////////////////////////////////

SQLITE_PRIVATE int parse_new_command(Parse *pParse, stored_proc* procedure, int type, char** psql);

SQLITE_PRIVATE int parseDeclareStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseSetStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseReturnStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseRaiseStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseAssertStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);

SQLITE_PRIVATE int parseIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseElseIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseElseStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseEndIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);

SQLITE_PRIVATE int parseLoopStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseEndLoopStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseBreakStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);
SQLITE_PRIVATE int parseContinueStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);

SQLITE_PRIVATE int parseForEachStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql);


#ifdef SQLITE_DEBUG
// returns the command type name in string format
SQLITE_PRIVATE char* command_type_str(int type) {
    switch (type) {
        case CMD_TYPE_DECLARE:
            return "DECLARE";
        case CMD_TYPE_SET:
            return "SET";
        case CMD_TYPE_RETURN:
            return "RETURN";
        case CMD_TYPE_RAISE:
            return "RAISE";
        case CMD_TYPE_ASSERT:
            return "ASSERT";

        case CMD_TYPE_STATEMENT:
            return "STATEMENT";

        case CMD_TYPE_IF:
            return "IF";
        case CMD_TYPE_ELSEIF:
            return "ELSEIF";
        case CMD_TYPE_ELSE:
            return "ELSE";
        case CMD_TYPE_ENDIF:
            return "END IF";

        case CMD_TYPE_LOOP:
            return "LOOP";
        case CMD_TYPE_ENDLOOP:
            return "ENDLOOP";
        case CMD_TYPE_BREAK:
            return "BREAK";
        case CMD_TYPE_CONTINUE:
            return "CONTINUE";
        case CMD_TYPE_FOREACH:
            return "FOREACH";
    }
    return "UNKNOWN";
}
#endif

// new_command using arrays:
// - allocate an array of 16 commands if the array is not yet allocated (cmds == NULL),
// - scan the array for the first empty slot,
// - if the array is full (no empty slot), reallocates the array to double its size,
// - stores the new command object at the empty slot,
// - returns the position of the new command object in the array.

SQLITE_PRIVATE int new_command(stored_proc* procedure, int type) {
    if (procedure->cmds == NULL) {
        procedure->cmds = (command*) sqlite3_malloc(16 * sizeof(command));
        if (procedure->cmds == NULL) return -1;
        memset(procedure->cmds, 0, 16 * sizeof(command));
        procedure->num_alloc_cmds = 16;
    } else if (procedure->num_cmds == procedure->num_alloc_cmds) {
        unsigned int old_size = procedure->num_alloc_cmds * sizeof(command);
        char *new_list = sqlite3_realloc(procedure->cmds, 2 * old_size);
        if (new_list == NULL) return -1;
        memset(new_list + old_size, 0, old_size);
        procedure->cmds = (command*) new_list;
        procedure->num_alloc_cmds *= 2;
    }
    int pos = procedure->num_cmds;
    procedure->num_cmds++;
    procedure->cmds[pos].type = type;
    procedure->cmds[pos].procedure = procedure;
    return pos;
}


////////////////////////////////////////////////////////////////////////////////
// PROCEDURE PARSING
////////////////////////////////////////////////////////////////////////////////

/*
** Parse a stored procedure.
** The SQL command must start with "PROCEDURE". The "CREATE [OR REPLACE]" is not stored.
*/
SQLITE_PRIVATE int parseStoredProcedure(Parse *pParse, stored_proc* procedure, char** psql) {
    char* sql = *psql;
    int rc = SQLITE_OK;
    int n, tokenType, i;
    sqlite3_var *varList, *var;

    if (sqlite3_strnicmp(sql, "PROCEDURE ", 10) == 0) {
      //procedure->is_function = false;
      // skip the "PROCEDURE" keyword + the space
      sql += 10;
    } else if (sqlite3_strnicmp(sql, "FUNCTION ", 9) == 0) {
      procedure->is_function = true;
      // skip the "FUNCTION" keyword + the space
      sql += 9;
    } else {
      goto loc_invalid;
    }

    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // get the procedure name
    n = sqlite3GetToken((u8*)sql, &tokenType);
    if( tokenType!=TK_ID || n == 0 ){
      goto loc_invalid;
    }
    // check the length of the procedure name
    if( n > sizeof(procedure->name)-1 ){
      sqlite3ErrorMsg(pParse, "procedure name too long");
      goto loc_invalid;
    }
    // store it into the procedure object
    strncpy(procedure->name, sql, n);
    procedure->name[n] = '\0';
    sql += n;

    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // check for the "(" character
    if (*sql != '(') {
      goto loc_invalid;
    }
    // skip the "(" character
    sql++;

    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // if there is no parameter, skip the ")" character
    if (*sql == ')') {
      goto loc_skip_params;
    }

    // parse the procedure parameters
    rc = parse_variables_list(procedure, VAR_POS_PARAMETER, &sql,
                              &procedure->num_params, &varList);
    if (rc != SQLITE_OK) {
        goto loc_invalid;
    }

    // store the list of parameters into the procedure object
    procedure->params = sqlite3_malloc( procedure->num_params * sizeof(sqlite3_var*) );
    for( i=0, var=varList; var; i++, var=var->nextUsed ){
      procedure->params[i] = var;
    }

    // check for the ")" character
    if (*sql != ')') {
        goto loc_invalid;
    }
loc_skip_params:
    // skip the ")" character
    sql++;

    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // check for the "BEGIN" keyword
    if (sqlite3_strnicmp(sql, "BEGIN", 5) != 0 || !sqlite3Isspace(sql[5])) {
        goto loc_invalid;
    }
    // skip the "BEGIN" keyword
    sql += 5;

    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // parse the procedure body
    rc = parse_procedure_body(pParse, procedure, &sql);
    if (rc != SQLITE_OK) {
        goto loc_invalid;
    }

    // check for the "END" keyword
    if (sqlite3_strnicmp(sql, "END", 3) != 0) {
        goto loc_invalid;
    }
    // skip the "END" keyword
    sql += 3;
    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;
    // check for the ";" character or the end of the SQL command
    if (*sql == ';') {
        sql++;
    } else if (*sql != '\0') {
        goto loc_invalid;
    }
    // skip spaces
    while (sqlite3Isspace(*sql)) sql++;

    // return the remaining SQL command
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    if (pParse->zErrMsg == NULL) {
      if (procedure->error_msg != NULL) {
        sqlite3ErrorMsg(pParse, "%s", procedure->error_msg);
        sqlite3_free(procedure->error_msg);
        procedure->error_msg = NULL;
      } else {
        sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
      }
    }
    return rc;
}

/*
** Parse the body of a stored procedure or function.
*/
SQLITE_PRIVATE int parse_procedure_body(Parse *pParse, stored_proc* procedure, char **psql) {
    char* sql = *psql;
    int rc = SQLITE_OK;

    while (sql && *sql) {

        // if the SQL command starts with "DECLARE", then it is a variable declaration
        if (sqlite3_strnicmp(sql, "DECLARE", 7) == 0 && sqlite3Isspace(sql[7])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_DECLARE, &sql);

        // if the SQL command starts with "SET", then it is a variable assignment
        } else if (sqlite3_strnicmp(sql, "SET", 3) == 0 && sqlite3Isspace(sql[3])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_SET, &sql);

        // if the SQL command starts with "RETURN", then it is a return statement
        } else if (sqlite3_strnicmp(sql, "RETURN", 6) == 0) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_RETURN, &sql);

        // if the SQL command starts with "RAISE", then it is a raise statement
        } else if (sqlite3_strnicmp(sql, "RAISE", 5) == 0) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_RAISE, &sql);

        // if the SQL command starts with "ASSERT", then it is an assert statement
        } else if (sqlite3_strnicmp(sql, "ASSERT", 5) == 0) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_ASSERT, &sql);

        // process IF, ELSEIF, ELSE and END IF
        } else if (sqlite3_strnicmp(sql, "IF", 2) == 0 && sqlite3Isspace(sql[2])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_IF, &sql);
        } else if (sqlite3_strnicmp(sql, "ELSEIF", 6) == 0 && sqlite3Isspace(sql[6])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_ELSEIF, &sql);
        } else if (sqlite3_strnicmp(sql, "ELSE", 4) == 0 && sqlite3Isspace(sql[4])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_ELSE, &sql);
        } else if (sqlite3_strnicmp(sql, "END IF;", 7) == 0) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_ENDIF, &sql);

        // process LOOP, ENDLOOP, BREAK, CONTINUE and FOREACH
        } else if (sqlite3_strnicmp(sql, "LOOP", 4) == 0 && sqlite3Isspace(sql[4])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_LOOP, &sql);
        } else if (sqlite3_strnicmp(sql, "END LOOP;", 9) == 0) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_ENDLOOP, &sql);
        } else if (sqlite3_strnicmp(sql, "BREAK", 5) == 0 && !sqlite3Isalpha(sql[5])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_BREAK, &sql);
        } else if (sqlite3_strnicmp(sql, "CONTINUE", 8) == 0 && !sqlite3Isalpha(sql[8])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_CONTINUE, &sql);
        } else if (sqlite3_strnicmp(sql, "FOREACH", 7) == 0 && sqlite3Isspace(sql[7])) {
            rc = parse_new_command(pParse, procedure, CMD_TYPE_FOREACH, &sql);

        // if the statement is just "END", then it is the end of the procedure
        } else if (sqlite3_strnicmp(sql, "END", 3) == 0 && !sqlite3Isalpha(sql[3])) {
            break;

        // if the SQL command is not one of the above, then it is a SQL statement
        } else {
            int pos = new_command(procedure, CMD_TYPE_STATEMENT);
            if (pos < 0) return SQLITE_NOMEM;
            command* cmd = &procedure->cmds[pos];
            // save the SQL statement
            cmd->sql = sql;
            cmd->nsql = skip_sql_command(&sql);
        }

        if (rc != SQLITE_OK) {
          if (procedure->error_msg != NULL) {
            // if an error occurred, set the error message and return
            sqlite3ErrorMsg(pParse, "%s", procedure->error_msg);
            // release the memory allocated for the error message
            sqlite3_free(procedure->error_msg);
            procedure->error_msg = NULL;
          }
          // exit the loop
          goto loc_exit;
        }

        // skip whitespaces
        while (sqlite3Isspace(*sql)) sql++;
    }

#ifdef SQLITE_DEBUG
    // print the list of commands
    for (int i = 0; i < procedure->num_cmds; i++) {
        command* cmd = &procedure->cmds[i];
        printf("command type: %s\n", command_type_str(cmd->type));
        if (cmd->sql) {
            printf("\tSQL: %.*s\n", cmd->nsql, cmd->sql);
        }
    }
#endif

loc_exit:
    // return the remaining SQL
    *psql = sql;
    return rc;
}

/*
** Parses a list of variables separated by comma, optionally containing a type.
** Used in these cases:
**   - procedure parameters: (@name TEXT, @value INT)
**   - SET statement: SET @name, @value = ...
**   - DECLARE statement: DECLARE @name, @value
**   - RETURN statement:  RETURN @name, @value
**   - FOREACH statement: FOREACH @name, @value IN ...
*/
SQLITE_PRIVATE int parse_variables_list(
  stored_proc* procedure,
  int cmd_pos,
  char** psql,
  unsigned int *pnum_vars,
  sqlite3_var **pvar_list
){
  char* sql = *psql;
  int rc = SQLITE_OK;
  int num_vars = 0;
  int n;
  int tokenType;
  sqlite3_var *varList=NULL, *lastVar=NULL, *var, *v;
  bool expect_type = false;

  /* can it contain a variable type? */
  if( cmd_pos==VAR_POS_PARAMETER || procedure->cmds[cmd_pos].type==CMD_TYPE_DECLARE ){
    /* yes */
    expect_type = true;
  }

  // iterate over the list of variables and clear the nextUsed pointer
  for(var=procedure->vars; var; var=var->next){
    var->nextUsed = NULL;
  }

  while (sql && *sql) {

    /* skip spaces */
    while( sqlite3Isspace(*sql) ) sql++;

    /* parse the variable name */
    n = sqlite3GetToken((u8*)sql, &tokenType);
    if( tokenType!=TK_VARIABLE ){
      goto loc_invalid_token;
    }
    //name = sql;
    //name_len = n;
    //sql += n;

    /* create a new variable or retrieve existing */
    //var = addVariable(procedure, cmd_pos, sql, n, NULL);
    var = addVariable(procedure, sql, n, 0, NULL);
    if( !var ) return SQLITE_ERROR;
    /* check if already on the list of used variables */
    for( v=varList; v; v=v->nextUsed ){
      if( v==var ){
        procedure->error_msg = sqlite3_mprintf("variable can only be used once: %s", v->name);
        goto loc_invalid_token;
      }
    }
    /* add it to the list of used variables */
    var->nextUsed = NULL;
    if( lastVar )
      lastVar->nextUsed = var;
    else
      varList = var;
    lastVar = var;
    num_vars++;

    /* skip the variable name */
    sql += n;

    /* skip spaces */
    while( sqlite3Isspace(*sql) ) sql++;

    /* parse the next token */
    n = sqlite3GetToken((u8*)sql, &tokenType);

    /* is it a variable type? */
    if( tokenType==TK_ID && expect_type ){
      /* type is specified */
      char *type = sql;
      int type_len = n;
      //! var->type = sqlite3StrNDup(type, type_len);
      /* skip the variable type */
      sql += n;
      /* get the next token */
      n = sqlite3GetToken((u8*)sql, &tokenType);
    }

    if( tokenType==TK_COMMA ){
      sql += n;
    }else{
      break;
    }
  }

  /* skip spaces */
  while( sqlite3Isspace(*sql) ) sql++;

  *psql = sql;
  *pnum_vars = num_vars;
  if( pvar_list ) *pvar_list = varList;
  return SQLITE_OK;

loc_invalid_token:
  if (rc == SQLITE_OK) rc = SQLITE_ERROR;
  if (procedure->error_msg == NULL) {
    procedure->error_msg = sqlite3_mprintf("invalid token: %s", sql);
  }
  *psql = sql;
  return rc;
}

SQLITE_PRIVATE int parse_new_command(Parse *pParse, stored_proc* procedure, int type, char** psql) {

    int pos = new_command(procedure, type);
    if (pos < 0) return SQLITE_NOMEM;
    //command* cmd = procedure->cmds[pos];

    switch (type) {
        case CMD_TYPE_DECLARE:
            return parseDeclareStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_SET:
            return parseSetStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_RETURN:
            return parseReturnStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_RAISE:
            return parseRaiseStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_ASSERT:
            return parseAssertStatement(pParse, procedure, pos, psql);

        case CMD_TYPE_IF:
            return parseIfStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_ELSEIF:
            return parseElseIfStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_ELSE:
            return parseElseStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_ENDIF:
            return parseEndIfStatement(pParse, procedure, pos, psql);

        case CMD_TYPE_LOOP:
            return parseLoopStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_ENDLOOP:
            return parseEndLoopStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_BREAK:
            return parseBreakStatement(pParse, procedure, pos, psql);
        case CMD_TYPE_CONTINUE:
            return parseContinueStatement(pParse, procedure, pos, psql);

        case CMD_TYPE_FOREACH:
            return parseForEachStatement(pParse, procedure, pos, psql);

        default:
            return SQLITE_ERROR;
    }

//! TODO: add common error handler

    return SQLITE_OK;
}

// ....

SQLITE_PRIVATE int parseDeclareStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;
    unsigned int num_vars;

    // skip "DECLARE" and whitespaces
    sql += 7;
    while (sqlite3Isspace(*sql)) sql++;

    // parse the list of variables
    int rc = parse_variables_list(procedure, pos, &sql, &num_vars, NULL);
    if (rc != SQLITE_OK) {
        goto loc_invalid;
    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // check if the statement ends with a semicolon
    if (*sql != ';') {
        goto loc_invalid;
    }
    // skip the semicolon
    sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    if (pParse->zErrMsg == NULL) {
      sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
    }
    *psql = sql;
    return rc;
}

/*
** Process special command:
**  SET @variable = {expression}
**  SET @name, @email, @phone = SELECT ...
**  SET @users = (SELECT * FROM users WHERE ...)
**  SET @list = ARRAY ('AA', 'BB', 'CC')
*/
SQLITE_PRIVATE int parseSetStatement(
  Parse *pParse, stored_proc* procedure, int pos, char** psql
){
  command *cmd = &procedure->cmds[pos];
  char *sql = *psql;
  int n, tokenType;
  int rc = SQLITE_OK;
  int i;
  sqlite3_var *var, *used_vars;

  XTRACE("parsing: %s\n", (char*)sql);

  /* skip "SET" and whitespaces */
  sql += 4;
  while( sqlite3Isspace(*sql) ) sql++;

  /* parse the variables */
  rc = parse_variables_list(procedure, pos, &sql, &cmd->num_vars, &used_vars);
  if (rc != SQLITE_OK) {
      goto loc_invalid;
  }

  /* We expect an equal sign now */
  if( *sql != '=' ){
    goto loc_invalid;
  }
  sql++;

  /* Check for spaces */
  while( sqlite3Isspace(*sql) ) sql++;

  /* Read the next token */
  n = sqlite3GetToken((u8*)sql, &tokenType);

  //if( tokenType==TK_ARRAY ){
  if( n==5 && sqlite3_strnicmp(sql, "ARRAY", 5)==0 ){
    // store the entire array in a single variable
    cmd->flags |= CMD_FLAG_STORE_AS_ARRAY;
    // skip the ARRAY token
    sql += n;
    // parse the array values into the command's input array
    rc = parse_input_array(pParse, procedure, pos, &sql);
    if (rc != SQLITE_OK) {
      goto loc_invalid;
    }
    // check for ';' at the end of the statement
    if( *sql != ';' ){
      goto loc_invalid;
    }
    // skip the semicolon
    sql++;
  }else{
    // if it is an open parenthesis, then the result is stored as an array
    if( tokenType==TK_LP ){
      // store the entire result set in a single variable
      cmd->flags |= CMD_FLAG_STORE_AS_ARRAY;
      // get the next token
      sql += n;
      while( sqlite3Isspace(*sql) ) sql++;
    }
    // store the SQL statement to be executed
    cmd->sql = sql;
    cmd->nsql = skip_sql_command(&sql);
    // if the command is enclosed in parenthesis, like this:
    // SET @users = (SELECT * FROM users WHERE ...);
    // then skip the closing parenthesis
    if (cmd->flags & CMD_FLAG_STORE_AS_ARRAY) {
      if( cmd->sql[cmd->nsql-1] != ')' ){
        sqlite3ErrorMsg(pParse, "expected ')'");
        goto loc_invalid;
      }
      cmd->nsql--;
    }
    // if the result set should be stored in a single variable
    if (cmd->flags & CMD_FLAG_STORE_AS_ARRAY) {
      // we expect a single variable to store the result set
      if (cmd->num_vars != 1) {
        sqlite3ErrorMsg(pParse, "number of variables must be 1");
        goto loc_invalid;
      }
    }
  }

  // store the list of used variables
  cmd->vars = sqlite3_malloc( cmd->num_vars * sizeof(sqlite3_var*) );
  if( !cmd->vars ) return SQLITE_NOMEM;
  for( i=0, var=used_vars; var; i++, var=var->nextUsed ){
    cmd->vars[i] = var;
  }

  /* skip spaces */
  while( sqlite3Isspace(*sql) ) sql++;

  /* return the updated parsing position */
  *psql = sql;

  return SQLITE_OK;
loc_invalid:
  if (rc == SQLITE_OK) rc = SQLITE_ERROR;
  if (pParse->zErrMsg == NULL) {
    sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
  }
  *psql = sql;
  return rc;
}

SQLITE_PRIVATE int parseReturnStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char *sql = *psql;
    char *expression;
    int rc;
    int i;
    sqlite3_var *var, *used_vars;

    // skip "RETURN" and whitespaces
    sql += 6;
    while (sqlite3Isspace(*sql)) sql++;

    // if there is nothing to return, then skip the semicolon
    if (*sql == ';') goto loc_skip_semicolon;

    // keep a copy of the expression
    expression = sql;

    // parse the list of variables
    rc = parse_variables_list(procedure, pos, &sql, &cmd->num_vars, &used_vars);
    if (rc == SQLITE_OK && *sql == ';') {

        // store the list of used variables
        cmd->vars = sqlite3_malloc( cmd->num_vars * sizeof(sqlite3_var*) );
        if( !cmd->vars ) return SQLITE_NOMEM;
        for( i=0, var=used_vars; var; i++, var=var->nextUsed ){
          cmd->vars[i] = var;
        }

loc_skip_semicolon:
        // skip the semicolon
        sql++;

    } else {

        rc = SQLITE_OK;
        if( procedure->error_msg ){
          sqlite3_free(procedure->error_msg);
          procedure->error_msg = NULL;
        }

        // parse the expression
        sql = expression;
        cmd->sql = sql;
        cmd->nsql = skip_sql_command(&sql);

    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    if (pParse->zErrMsg == NULL) {
      sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
    }
    *psql = sql;
    return rc;
}

/*
** Parse a RAISE statement.
*/
SQLITE_PRIVATE int parseRaiseStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char *sql = *psql;

    // skip "RAISE" and whitespaces
    sql += 5;
    while (sqlite3Isspace(*sql)) sql++;

    // check for "EXCEPTION"
    if (sqlite3_strnicmp(sql, "EXCEPTION", 9) != 0) {
      goto loc_invalid;
    }

    // skip "EXCEPTION" and whitespaces
    sql += 9;
    while (sqlite3Isspace(*sql)) sql++;

    // parse the expression
    cmd->sql = sql;
    cmd->nsql = skip_sql_command(&sql);

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    if (pParse->zErrMsg == NULL) {
      sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
    }
    *psql = sql;
    return SQLITE_ERROR;
}

/*
** Parse an ASSERT statement.
*/
SQLITE_PRIVATE int parseAssertStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char *sql = *psql;
    int rc = SQLITE_OK;

    // skip "ASSERT" and whitespaces
    sql += 6;
    while (sqlite3Isspace(*sql)) sql++;

    // store the condition expression
    cmd->sql = sql;
    cmd->nsql = skip_delimited_sql_command(&sql, TK_COMMA, 1);
    if (cmd->nsql < 0) {
        sqlite3ErrorMsg(pParse, "expected format: ASSERT condition, error message");
        return SQLITE_ERROR;
    }

    // store the error message expression
    cmd->sql2 = sql;
    cmd->nsql2 = skip_delimited_sql_command(&sql, TK_SEMI, 1);
    if (cmd->nsql2 < 0) {
        sqlite3ErrorMsg(pParse, "invalid error message in ASSERT statement");
        return SQLITE_ERROR;
    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}


// IF, ELSEIF, ELSE and END IF statements


// declare the if_block struct
typedef struct if_block {
    struct if_block* next;
    int first_cmd;
    int last_cmd;
} if_block;

// declare the if_block stack
if_block* if_stack = NULL;

SQLITE_PRIVATE int parseIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char* sql = *psql;

    // this is a new IF block, push a new IF block controller on the stack
    if_block* ifb = malloc(sizeof(if_block));
    if (!ifb) return SQLITE_NOMEM;
    memset(ifb, 0, sizeof(if_block));
    ifb->next = if_stack;
    if_stack = ifb;

    // store the first command position
    ifb->first_cmd = pos;
    ifb->last_cmd = pos;

    // skip "IF" and whitespaces
    sql += 2;
    while (sqlite3Isspace(*sql)) sql++;

    // get the condition
    cmd->sql = sql;
    cmd->nsql = skip_delimited_sql_command(&sql, TK_THEN, 4);

    if (cmd->nsql == -1) {
        sqlite3ErrorMsg(pParse, "expected THEN");
        return SQLITE_ERROR;
    }

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}

SQLITE_PRIVATE int parseElseIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char* sql = *psql;

    // get the current IF block controller from the stack
    if_block* ifb = if_stack;
    if (!ifb) {
        sqlite3ErrorMsg(pParse, "ELSEIF without IF");
        return SQLITE_ERROR;
    }

    // store the position of the IF command on this block
    cmd->related_cmd = ifb->first_cmd;

    // get the position of the last command in the IF block
    int last_if_cmd = ifb->last_cmd;

    // mark this as the next command in the current IF block
    procedure->cmds[last_if_cmd].next_if_cmd = pos;

    // store this as the last command in the current IF block
    ifb->last_cmd = pos;


    // skip "ELSEIF" and whitespaces
    sql += 6;
    while (sqlite3Isspace(*sql)) sql++;

    // get the condition
    cmd->sql = sql;
    cmd->nsql = skip_delimited_sql_command(&sql, TK_THEN, 4);

    if (cmd->nsql == -1) {
        sqlite3ErrorMsg(pParse, "expected THEN");
        return SQLITE_ERROR;
    }

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}

SQLITE_PRIVATE int parseElseStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command *cmd = &procedure->cmds[pos];
    char* sql = *psql;

    // get the current IF block controller from the stack
    if_block* ifb = if_stack;
    if (!ifb) {
      sqlite3ErrorMsg(pParse, "ELSE without IF");
      return SQLITE_ERROR;
    }

    // store the position of the IF command on this block
    cmd->related_cmd = ifb->first_cmd;

    // get the position of the last command in the IF block
    int last_if_cmd = ifb->last_cmd;

    // mark this as the next command in the current IF block
    procedure->cmds[last_if_cmd].next_if_cmd = pos;

    // store this as the last command in the current IF block
    ifb->last_cmd = pos;


    // skip "ELSE" and whitespaces
    sql += 4;
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}

SQLITE_PRIVATE int parseEndIfStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;

    // get the current IF block controller from the stack
    if_block* ifb = if_stack;
    if (!ifb) {
      sqlite3ErrorMsg(pParse, "END IF without IF");
      return SQLITE_ERROR;
    }

    // get the position of the last command in the IF block
    int last_if_cmd = ifb->last_cmd;

    // mark this as the next command in the current IF block
    procedure->cmds[last_if_cmd].next_if_cmd = pos;

    // pop the current IF block controller from the stack
    if_stack = ifb->next;
    free(ifb);


    // skip "END IF;" and whitespaces
    sql += 7;
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}


// LOOP, ENDLOOP and BREAK statements


// declare the loop_block struct
typedef struct loop_block {
    struct loop_block* next;
    int type;  // CMD_TYPE_LOOP or CMD_TYPE_FOREACH
    int start_cmd;
} loop_block;

// declare the loop_block stack
loop_block* loop_stack = NULL;

SQLITE_PRIVATE int parseLoopStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;

    // this is a new LOOP block, push a new LOOP block controller on the stack
    loop_block* loopb = malloc(sizeof(loop_block));
    if (!loopb) return SQLITE_NOMEM;
    memset(loopb, 0, sizeof(loop_block));
    loopb->next = loop_stack;
    loop_stack = loopb;

    loopb->type = CMD_TYPE_LOOP;
    loopb->start_cmd = pos;

    // skip "LOOP" and whitespaces
    sql += 4;
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}

SQLITE_PRIVATE int parseEndLoopStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;

    // get the current LOOP block controller from the stack
    loop_block* loopb = loop_stack;
    if (!loopb) {
      sqlite3ErrorMsg(pParse, "END LOOP without LOOP");
      return SQLITE_ERROR;
    }

    // pointers like this: (LOOP -> END LOOP) (END LOOP -> LOOP)

    // get the position of the loop start command
    int start_loop_pos = loopb->start_cmd;

    // store the position of the END LOOP command in the loop start command
    procedure->cmds[start_loop_pos].related_cmd = pos;

    // store the position of the loop start command in the END LOOP command
    procedure->cmds[pos].related_cmd = start_loop_pos;

    // pop the current LOOP block controller from the stack
    loop_stack = loopb->next;
    free(loopb);


    // skip "END LOOP;" and whitespaces
    sql += 9;
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
}

SQLITE_PRIVATE int parseBreakStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;

    // get the current LOOP block controller from the stack
    loop_block* loopb = loop_stack;
    if (!loopb) {
      sqlite3ErrorMsg(pParse, "BREAK statement without a LOOP block");
      return SQLITE_ERROR;
    }

    // skip "BREAK" and whitespaces
    sql += 5;
    while (sqlite3Isspace(*sql)) sql++;
    // get the loop level
    int level = 1;
    if (sqlite3Isdigit(*sql)) {
        level = atoi(sql);
    }

    // if deeper level, get the corresponding parent LOOP block controller
    while (level > 1) {
        loopb = loopb->next;
        if (!loopb){
            sqlite3ErrorMsg(pParse, "BREAK statement with invalid loop level");
            *psql = sql;
            return SQLITE_ERROR;
        }
        level--;
    }

    // pointers like this: BREAK -> LOOP -> END LOOP

    // get the position of the loop start command
    int start_loop_pos = loopb->start_cmd;

    // store the position of the loop start command in the BREAK command
    procedure->cmds[pos].related_cmd = start_loop_pos;


    if (sqlite3Isdigit(*sql)) {
        // skip the loop level and whitespaces
        while (sqlite3Isdigit(*sql)) sql++;
        while (sqlite3Isspace(*sql)) sql++;
    }

    // check for ';' at the end of the statement
    if (*sql != ';') goto loc_invalid;
    sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    *psql = sql;
    return SQLITE_ERROR;
}

SQLITE_PRIVATE int parseContinueStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    char* sql = *psql;

    // get the current LOOP block controller from the stack
    loop_block* loopb = loop_stack;
    if (!loopb) {
      sqlite3ErrorMsg(pParse, "CONTINUE statement without a LOOP block");
      *psql = sql;
      return SQLITE_ERROR;
    }

    // skip "CONTINUE" and whitespaces
    sql += 8;
    while (sqlite3Isspace(*sql)) sql++;

    // get the loop level
    int level = 1;
    if (sqlite3Isdigit(*sql)) {
        level = atoi(sql);
    }

    // if deeper level, get the corresponding parent LOOP block controller
    while (level > 1) {
        loopb = loopb->next;
        if (!loopb) {
            sqlite3ErrorMsg(pParse, "CONTINUE statement with invalid loop level");
            *psql = sql;
            return SQLITE_ERROR;
        }
        level--;
    }

    // the pointer is like this: CONTINUE -> LOOP

    // get the position of the loop start command
    int start_loop_pos = loopb->start_cmd;

    // store the position of the loop start command in the CONTINUE command
    procedure->cmds[pos].related_cmd = start_loop_pos;


    if (sqlite3Isdigit(*sql)) {
        // skip the loop level and whitespaces
        while (sqlite3Isdigit(*sql)) sql++;
        while (sqlite3Isspace(*sql)) sql++;
    }

    // check for ';' at the end of the statement
    if (*sql != ';') goto loc_invalid;
    sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;
loc_invalid:
    *psql = sql;
    return SQLITE_ERROR;
}

/*

parse FOREACH statements

example usage:

FOREACH @item IN ARRAY ('AA', 'BB', 'CC') DO
    statements;
END LOOP;

FOREACH @item IN @items DO
    statements;
END LOOP;

FOREACH @product_id, @quantity, @price IN @sale_items DO
    statements;
END LOOP;

FOREACH @product_id, @quantity, @price IN SELECT * FROM sale_items WHERE ... DO
    statements;
END LOOP;

FOREACH VALUE IN SELECT * FROM sale_items WHERE ... DO
    statements;
END LOOP;

*/

SQLITE_PRIVATE int parseForEachStatement(Parse *pParse, stored_proc* procedure, int pos, char** psql) {
    command* cmd = &procedure->cmds[pos];
    char* sql = *psql;
    int rc = SQLITE_OK;
    int i;
    int n, tokenType;
    sqlite3_var *var, *used_vars;

    // this is a new loop block, push a new loop block controller onto the stack
    loop_block* loopb = malloc(sizeof(loop_block));
    if (!loopb) return SQLITE_NOMEM;
    memset(loopb, 0, sizeof(loop_block));
    loopb->next = loop_stack;
    loop_stack = loopb;

    loopb->type = CMD_TYPE_FOREACH;
    loopb->start_cmd = pos;

    // parse the FOREACH statement

    // skip "FOREACH" and whitespaces
    sql += 7;
    while (sqlite3Isspace(*sql)) sql++;

    // check if the next token is VALUE
    if (sqlite3_strnicmp(sql, "VALUE", 5) == 0 && sqlite3Isspace(sql[5])) {
        // skip "VALUE" and whitespaces
        sql += 6;
        while (sqlite3Isspace(*sql)) sql++;
    } else {
      // parse the list of variables
      rc = parse_variables_list(procedure, pos, &sql, &cmd->num_vars, &used_vars);
      if (rc != SQLITE_OK) {
          goto loc_invalid;
      }
      // store the list of used variables
      cmd->vars = sqlite3_malloc( cmd->num_vars * sizeof(sqlite3_var*) );
      if( !cmd->vars ) return SQLITE_NOMEM;
      for( i=0, var=used_vars; var; i++, var=var->nextUsed ){
        cmd->vars[i] = var;
      }
    }

    // skip "IN" and whitespaces
    if (sqlite3_strnicmp(sql, "IN", 2) != 0 || !sqlite3Isspace(sql[2])) {
      goto loc_invalid;
    }
    sql += 3;
    while (sqlite3Isspace(*sql)) sql++;

    // check if the next token is ARRAY or a variable or a SQL statement
    // use an external function to parse the ARRAY or variable
    // save the result in the loop block controller
    if (sqlite3_strnicmp(sql, "ARRAY", 5) == 0) {
        // skip "ARRAY"
        sql += 5;
        // parse the array values into the command's input array
        rc = parse_input_array(pParse, procedure, pos, &sql);
        if (rc != SQLITE_OK) {
            goto loc_invalid;
        }
    } else {
        // get the next token
        n = sqlite3GetToken((u8*)sql, &tokenType);
        if (tokenType == TK_VARIABLE) {
            // get the input variable, that must exist
            var = findVariable(procedure, sql, n);
            if (!var) {
                goto loc_invalid;
            }
            // save the pointer to the input variable in the command
            cmd->input_var = var;
            // skip the variable name
            sql += n;
        } else {
            // get the SQL statement
            cmd->sql = sql;
            cmd->nsql = skip_delimited_sql_command(&sql, TK_DO, 2);
            if (cmd->nsql < 0) {
                goto loc_invalid;
            }
            goto loc_skip_keyword;
        }
    }

    if (cmd->num_vars==0 && cmd->sql==NULL) {
      sqlite3ErrorMsg(pParse, "FOREACH VALUE can only be used with SQL statements");
      goto loc_invalid;
    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // check for "DO"
    if (sqlite3_strnicmp(sql, "DO", 2) != 0 || !sqlite3Isspace(sql[2])) {
      goto loc_invalid;
    }
    sql += 3;
loc_skip_keyword:
    while (sqlite3Isspace(*sql)) sql++;

    // store the current parsing position on the psql pointer
    *psql = sql;

    return SQLITE_OK;

loc_invalid:
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    if (pParse->zErrMsg == NULL) {
      sqlite3ErrorMsg(pParse, "invalid token: %s", sql);
    }
    *psql = sql;
    return rc;
}

/*
** Parse an array and save it in the command's input array
*/
SQLITE_PRIVATE int parse_input_array(
  Parse *pParse, stored_proc* procedure, int pos, char** psql
){
    command* cmd = &procedure->cmds[pos];
    sqlite3_array* array = NULL;
    int rc;

    // parse the array values into an sqlite3_array object
    rc = parse_array(pParse, procedure, psql, &array);
    if (rc != SQLITE_OK) {
        return rc;
    }

    // save the array in the command
    cmd->input_array = array;

    return SQLITE_OK;
}

////////////////////////////////////////////////////////////////////////////////
// PROCEDURE "COMPILATION"
////////////////////////////////////////////////////////////////////////////////

/*
** Compile the stored procedure into a prepared statement
*/
SQLITE_PRIVATE void prepareNewStoredProcedure(Parse *pParse, char **psql, bool or_replace) {
    stored_proc* procedure = NULL;
    char* sql = *psql;
    char* end;
    int nsql;
    int rc = SQLITE_OK;

    procedure = (stored_proc*) sqlite3MallocZero(sizeof(stored_proc));
    if (procedure == NULL) { rc = SQLITE_NOMEM; goto loc_exit; }
    procedure->db = pParse->db;

    // parse the stored procedure, to check for syntax errors
    rc = parseStoredProcedure(pParse, procedure, psql);
    //releaseProcedure(procedure);
    if (rc != SQLITE_OK) {
      goto loc_exit;
    }

    // remove the last ';' and keep only up to 'END'
    end = (*psql) - 1;
    while( end > sql && sqlite3Isspace(*end) ) end--;
    if( end > sql && *end==';' ) end--;
    while( end > sql && sqlite3Isspace(*end) ) end--;
    // compute the length of the SQL statement
    nsql = (int)(end - sql + 1);

    // creates and return a prepared statement, to be executed later

#if 0
    // sqlite3NestedParse() cannot be used because the table does not exist
    // at this point and the INSERT command cannot be prepared

    // create the aergolite_stored_procedures table if it does not exist
    sqlite3NestedParse(pParse,
        "CREATE TABLE IF NOT EXISTS aergolite_stored_procedures ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "is_function BOOL NOT NULL,"
        "code TEXT NOT NULL"
        ")");

    // insert the stored procedure in the database
    // if the `or_replace` parameter is true, use the `OR REPLACE` keyword
    sqlite3NestedParse(pParse,
        "INSERT %s INTO aergolite_stored_procedures (name, is_function, code)"
        " VALUES (%Q, %d, %Q)",
        or_replace ? "OR REPLACE" : "",
        procedure->name,
        procedure->is_function,
        sql);
#endif

    Vdbe *v = sqlite3GetVdbe(pParse);
    if (v == NULL) { rc = SQLITE_NOMEM; goto loc_exit; }

    sqlite3BeginWriteOperation(pParse, 0, 0);

    // create the aergolite_stored_procedures table if it does not exist
    const char *sql2 =
        "CREATE TABLE IF NOT EXISTS aergolite_stored_procedures ("
        "name TEXT PRIMARY KEY,"
        "is_function BOOL NOT NULL,"
        "code TEXT NOT NULL"
        ")";
    sqlite3VdbeAddOp4(v, OP_SqlExec, 0, 0, 0, sql2, P4_STATIC);

    // insert the stored procedure in the database
    sql2 = sqlite3_mprintf(
        "INSERT %s INTO aergolite_stored_procedures (name, is_function, code)"
        " VALUES (%Q, %d, %.*Q)",
        or_replace ? "OR REPLACE" : "",
        procedure->name,
        procedure->is_function,
        nsql, sql);
    sqlite3VdbeAddOp4(v, OP_SqlExec, 0, 0, 0, sql2, P4_DYNAMIC);

    // finish coding the VDBE program
    sqlite3FinishCoding(pParse);

    XTRACE("insertion SQL: %s\n", sql2);

loc_exit:
    if( procedure ){
      releaseProcedure(procedure);
    }
    if( rc!=SQLITE_OK ){
      pParse->rc = rc;
      pParse->nErr++;
    }
}

/*
** Get a stored procedure from the database
*/
SQLITE_PRIVATE int getStoredProcedure(sqlite3* db, char* name, int name_len, char** pcode) {
    sqlite3_stmt* stmt = NULL;
    int rc = SQLITE_OK;
    char* code = NULL;

    // prepare the statement
    rc = sqlite3_prepare_v2(db,
                            "SELECT code FROM aergolite_stored_procedures WHERE name = ?",
                            -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto loc_exit;
    }

    // bind the name parameter
    rc = sqlite3_bind_text(stmt, 1, name, name_len, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        goto loc_exit;
    }

    // execute the statement
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // get the code
        code = sqlite3StrDup((char*)sqlite3_column_text(stmt, 0));
        rc = SQLITE_OK;
    } else if (rc == SQLITE_DONE) {
        // no stored procedure found
        rc = SQLITE_NOTFOUND;
    }

loc_exit:
    if (stmt != NULL) {
        sqlite3_finalize(stmt);
    }
    *pcode = code;
    return rc;
}

/*
** Parse a stored procedure call
*/
SQLITE_PRIVATE int parseProcedureCall(
    Parse *pParse, char** psql, char** name, int* name_len, sqlite3_array** pparams
){
    char* sql = *psql;
    int n;
    int rc = SQLITE_OK;
    int tokenType;
    sqlite3_array* array = NULL;

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // get the procedure name
    n = sqlite3GetToken((u8*)sql, &tokenType);
    if (tokenType != TK_ID) {
        goto loc_invalid;
    }
    // return the procedure name and length
    *name = sql;
    *name_len = n;
    // skip the procedure name
    sql += n;

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // parse the input parameters into an sqlite3_array object
    rc = parse_array(pParse, NULL, &sql, &array);
    if (rc != SQLITE_OK) {
        goto loc_invalid;
    }

    // skip whitespaces
    while (sqlite3Isspace(*sql)) sql++;

    // return the current parsing position on the psql pointer
    *psql = sql;
    // return the input parameters array
    *pparams = array;

    return SQLITE_OK;

loc_invalid:
    *psql = sql;
    if (rc == SQLITE_OK) rc = SQLITE_ERROR;
    return rc;
}

/*
** Process the input parameters of a stored procedure call
** 
*/
SQLITE_PRIVATE int processCallParameters(Parse *pParse, sqlite3_array *input_array) {
    int count = 0;
    int i;

    // count how many variables are there in the input array
    for (i = 0; i < input_array->num_items; i++) {
        if (is_variable(&input_array->value[i])) {
            count++;
        }
    }

    if (count > 0) {
        Expr aExpr[1];
        Expr *pExpr = &aExpr[0];
        // iterate the input_array
        for (i = 0; i < input_array->num_items; i++) {
            sqlite3_value *value = &input_array->value[i];
            // check if the current value is a variable
            if (is_variable(value)) {
                // set the pExpr->u.zToken to the variable name
                memset(pExpr, 0, sizeof(Expr));
                pExpr->u.zToken = value->z;
                // assign a number to the variable
                sqlite3ExprAssignVarNumber(pParse, pExpr, (u32)value->n);
                // replace the variable identifier (string) with the variable number
                sqlite3VdbeMemSetInt64(value, pExpr->iColumn);
                // mark it as a variable
                value->eSubtype = 'v';
            }
        }
    }

    return SQLITE_OK;
}

/*
** Compile the stored procedure call into a prepared statement
*/
SQLITE_PRIVATE void prepareProcedureCall(Parse *pParse, char **psql) {
    sqlite3 *db = pParse->db;
    procedure_call *call = NULL;
    stored_proc *procedure = NULL;
    char *sql = *psql;
    char *name, *code=NULL, *code2;
    int name_len;
    int rc = SQLITE_OK;
    Vdbe *v = NULL;

    call = (procedure_call*) sqlite3MallocZero(sizeof(procedure_call));
    if (call == NULL) { rc = SQLITE_NOMEM; goto loc_exit; }

    // parse the CALL statement
    rc = parseProcedureCall(pParse, &sql, &name, &name_len, &call->input_array);
    if (rc != SQLITE_OK) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Invalid token in stored procedure call: %s", sql);
      }
      goto loc_exit;
    }

    // get the stored procedure from the database
    rc = getStoredProcedure(db, name, name_len, &code);
    // if the stored procedure does not exist, return an error
    if (rc == SQLITE_NOTFOUND || code == NULL) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Stored procedure not found: %.*s", name_len, name);
      }
      goto loc_exit;
    }
    if (rc != SQLITE_OK) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Error loading stored procedure: %s", sqlite3_errmsg(db));
      }
      goto loc_exit;
    }

    // process the variables in the input array
    rc = processCallParameters(pParse, call->input_array);
    if (rc != SQLITE_OK) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Error processing stored procedure parameters: %s",
              sqlite3_errmsg(db));
      }
      goto loc_exit;
    }

    // allocate a new stored_proc object
    procedure = (stored_proc*) sqlite3MallocZero(sizeof(stored_proc));
    if (procedure == NULL) {
      rc = SQLITE_NOMEM;
      goto loc_exit;
    }
    procedure->db = pParse->db;
    procedure->code = code;

    // parse the stored procedure to be executed
    code2 = code;
    rc = parseStoredProcedure(pParse, procedure, &code2);
    if (rc != SQLITE_OK) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Error parsing stored procedure: %s",
              sqlite3_errmsg(db));
      }
      goto loc_exit;
    }

    // the CALL command cannot be used with functions
    if (procedure->is_function) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Cannot call a function: %.*s", name_len, name);
      }
      pParse->rc = SQLITE_PERM;
      rc = SQLITE_PERM;
      goto loc_exit;
    }

    // check the number of parameters
    if (call->input_array->num_items != procedure->num_params) {
      rc = SQLITE_ERROR;
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Invalid number of parameters: %d",
              call->input_array->num_items);
      }
      goto loc_exit;
    }

#if 0
    // check the parameters of the CALL statement
    rc = checkProcedureCall(call, procedure);
    if (rc != SQLITE_OK) {
      if (pParse->zErrMsg == NULL) {
        sqlite3ErrorMsg(pParse, "Error checking procedure call: %s",
              sqlite3_errmsg(db));
      }
      goto loc_exit;
    }
#endif

    // store the procedure object in the call object
    call->procedure = procedure;

    // compile the stored procedure call into a prepared statement
    v = sqlite3GetVdbe(pParse);
    if( !v ){
      rc = SQLITE_NOMEM;
      goto loc_exit;
    }
    sqlite3CodeVerifySchema(pParse, 0);
    // store the procedure call object in the Vdbe object
    v->pCall = call;
    // add the 3 opcodes
    sqlite3VdbeAddOp0(v, OP_CallProcedure);
    sqlite3VdbeAddOp0(v, OP_Noop);  /* replaced by a OP_ResultRow opcode */
    assert( POS_RESULT_ROW==sqlite3VdbeCurrentAddr(v)-1 );
    sqlite3VdbeAddOp0(v, OP_Noop);  /* replaced by a OP_NextResult opcode */
    assert( POS_NEXT_RESULT==sqlite3VdbeCurrentAddr(v)-1 );

    sqlite3FinishCoding(pParse);


loc_exit:

    *psql = sql;

    // errors can be set on execution using the sqlite3VdbeError() function

    if (rc != SQLITE_OK) {
      if (v) {
        v->pCall = NULL;
      }
      if (call) {
        call->procedure = NULL;
        releaseProcedureCall(call);
      }
      if (procedure) {
        procedure->code = NULL;
        releaseProcedure(procedure);
      }
      if (code) {
        sqlite3_free(code);
      }
      if (pParse->rc == SQLITE_OK) {
        pParse->rc = rc;
        pParse->nErr++;
      }
    }
}

// the prepare step should create a prepared statement with the stored procedure
// the execute step should execute the stored procedure by iterating and processing each command on the stored_proc object

////////////////////////////////////////////////////////////////////////////////
// PROCEDURE EXECUTION
////////////////////////////////////////////////////////////////////////////////

/*
** Execute an expression and return the result.
*/
SQLITE_PRIVATE int execute_expression(Vdbe *v, command *cmd, bool* bool_result){
  int rc = SQLITE_OK;
  sqlite3* db = v->db;

  // if the CMD_FLAG_DYNAMIC_SQL is not set
  if( (cmd->flags & CMD_FLAG_DYNAMIC_SQL)==0 ){
    // add "SELECT" to the expression
    cmd->sql = sqlite3_mprintf("SELECT %.*s", cmd->nsql, cmd->sql);
    cmd->nsql += 7;
    if( cmd->sql==NULL ) return SQLITE_NOMEM;
    // mark that this SQL command string is dynamically allocated
    cmd->flags |= CMD_FLAG_DYNAMIC_SQL;
  }

  if( cmd->stmt==NULL ){
    // prepare the expression
    rc = sqlite3_prepare_v2(db, cmd->sql, cmd->nsql, &cmd->stmt, NULL);
  } else {
    // reset the statement
    rc = sqlite3_reset(cmd->stmt);
  }
  if( rc ) goto loc_error;

  // bind variables
  bindLocalVariables(cmd->procedure, cmd->stmt);

  // execute the expression
  rc = sqlite3_step(cmd->stmt);
  if( rc!=SQLITE_ROW ){
    sqlite3VdbeError(v, "expression did not return a result");
    goto loc_error;
  }

  // get the result
  if( bool_result ) {
    *bool_result = sqlite3_column_int(cmd->stmt, 0);
  }else{
    sqlite3_var *var;
    // get the number of result columns
    int num_cols = sqlite3_column_count(cmd->stmt);
    if( num_cols==0 ){
      sqlite3VdbeError(v, "expression did not return a result");
      goto loc_error;
    }
    // allocate variables to store the result
    cmd->num_vars = num_cols;
    cmd->vars = sqlite3_malloc( num_cols * sizeof(sqlite3_var*) );
    if( !cmd->vars ) return SQLITE_NOMEM;
    // for each result column
    for(int i=0; i<num_cols; i++){
      char buf[32];
      // get the column name
      char *name = (char*) sqlite3_column_name(cmd->stmt, i);
      if( !name ){
        // if the column name is not available, use the column index
        sprintf(buf, "col%d", i+1);
        name = buf;
      }
      // create a new variable
      var = addVariable(cmd->procedure, name, strlen(name), 0, NULL);
      if( !var ) return SQLITE_NOMEM;
      cmd->vars[i] = var;
      // get the column value
      sqlite3_value *value = sqlite3_column_value(cmd->stmt, i);
      // move the column value to the variable
      sqlite3VdbeMemMove(&var->value, value);
    }
  }

  // make sure the statement returns no more rows
  rc = sqlite3_step(cmd->stmt);
  if( rc==SQLITE_ROW ){
    sqlite3VdbeError(v, "expression returned more than one row");
    goto loc_error;
  }else if( rc!=SQLITE_DONE ){
    goto loc_error;
  }

  return SQLITE_OK;
loc_error:
  if( v->zErrMsg==NULL ){
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
  }
  return rc;
}

SQLITE_PRIVATE int db_query_str(stored_proc *procedure, char *sql, char **presult){
  sqlite3* db = procedure->db;
  sqlite3_stmt *stmt = NULL;
  int rc = SQLITE_OK;

  // prepare the expression
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if( rc ) goto loc_exit;

  // bind variables
  bindLocalVariables(procedure, stmt);

  // execute the expression
  rc = sqlite3_step(stmt);
  if( rc!=SQLITE_ROW ){
    if( rc==SQLITE_DONE ){
      rc = SQLITE_ERROR;
    }
    goto loc_exit;
  }

  // get the result
  *presult = sqlite3StrDup((char*)sqlite3_column_text(stmt, 0));

  // make sure the statement returns no more rows
  rc = sqlite3_step(stmt);
  if( rc==SQLITE_ROW ){
    rc = SQLITE_ERROR;
  }else if( rc==SQLITE_DONE ){
    rc = SQLITE_OK;
  }

loc_exit:
  sqlite3_finalize(stmt);
  return rc;
}

/*
** Copy the values from the input array to the procedure parameters.
*/
SQLITE_PRIVATE void copyProcedureParameters(Vdbe *v, procedure_call *call) {
  stored_proc *procedure = call->procedure;
  sqlite3_array *input_array = call->input_array;
  int pos;

  assert(input_array->num_items == procedure->num_params);

  // iterate the input array
  for (pos = 0; pos < input_array->num_items; pos++) {
    // get the input value
    Mem *input = &input_array->value[pos];
    // get the parameter value
    Mem *param = &procedure->params[pos]->value;

    // check if the input value is a variable
    if (input->eSubtype == 'v' && (input->flags & MEM_Int)!=0) {
      assert(v->aVar!=0 && input->u.i>0 && input->u.i<=v->nVar);
      // get the variable number
      int var_number = input->u.i;
      // get the variable value
      Mem *var = &v->aVar[var_number - 1];
      // copy the variable value to the parameter
      sqlite3VdbeMemShallowCopy(param, var, MEM_Static);
    } else {
      // copy the input value to the parameter
      sqlite3VdbeMemShallowCopy(param, input, MEM_Static);
    }
#ifdef SQLITE_DEBUG
    printf("copyProcedureParameters() pos=%d value=", pos);
    memTracePrint(param);
    puts("");
#endif
  }

}

SQLITE_PRIVATE void sqlite3ChangeOpcode(Vdbe *v, int pos, int opcode, int p1, int p2){
  VdbeOp *pOp = &v->aOp[pos];
  pOp->opcode = opcode;
  pOp->p1 = p1;
  pOp->p2 = p2;
}

SQLITE_PRIVATE int sqlite3VdbeNextResult(Vdbe *v){
  stored_proc *procedure = v->pCall->procedure;
  int num_cols = 0;
  int i;

  // check if the procedure has a result set
  sqlite3_array *array = procedure->result_array;
  if( !array ){
    // no result set
    return SQLITE_DONE;
  }

  // increment the current row
  procedure->current_row++;
  // check if there are more rows to return
  if( procedure->current_row >= array->num_items ){
    // no more rows
    return SQLITE_DONE;
  }

  // get the array value
  Mem *row_value = &array->value[procedure->current_row];

  // check if it is an array
  array = get_array_from_value(row_value);
  if( array ){
    // copy the values from the array to the result set
    for( i=0; i<array->num_items; i++ ){
      // get the array value
      Mem *value = &array->value[i];
      // copy the value to the result set
      sqlite3VdbeMemShallowCopy(&v->aMem[i+1], value, MEM_Static);
    }
    num_cols = array->num_items;
  } else {
    // copy the value to the result set
    sqlite3VdbeMemMove(&v->aMem[1], row_value);
    //sqlite3VdbeMemShallowCopy(&v->aMem[1], row_value, MEM_Static);
    num_cols = 1;
  }

  sqlite3VdbeSetNumCols(v, num_cols);
  //sqlite3VdbeSetColName(v, 0, COLNAME_NAME, "rows deleted", SQLITE_STATIC);

  // update the result opcode with the number of columns
  sqlite3ChangeOpcode(v, POS_RESULT_ROW, OP_ResultRow, 1, num_cols);

  return SQLITE_ROW;
}

/*
** Execute a return command.
*/
SQLITE_PRIVATE int executeReturnCommand(Vdbe *v, command *cmd) {
  stored_proc *procedure = cmd->procedure;
  int rc = SQLITE_OK;
  int i;

  if( cmd->num_vars==0 && cmd->sql==NULL ){
    // no result set
    return SQLITE_OK;
  }

  assert( procedure!=NULL );

  if ( procedure->aMem==NULL && v->aMem!=NULL ) {
    // copy the aMem array to the procedure object
    procedure->aMem = v->aMem;
    procedure->nMem = v->nMem;
    // clear the aMem array from the Vdbe object
    v->aMem = NULL;
    v->nMem = 0;
  }

  // release memory from the previous result set
  if( v->aMem ){
    // release each memory cell
    for(i=0; i<v->nMem; i++){
      sqlite3VdbeMemRelease(&v->aMem[i]);
    }
    // release the memory
    sqlite3DbFree(v->db, v->aMem);
    v->aMem = NULL;
    v->nMem = 0;
  }

  // if it returns an expression
  if( cmd->sql!=NULL ){
    // evaluate the expression
    rc = execute_expression(v, cmd, NULL);
    if( rc ) return rc;
  }

  // if returning a result set (many rows)
  if( cmd->num_vars==1 && is_array(&cmd->vars[0]->value) ){
    // get the array
    sqlite3_array *array = get_array_from_value(&cmd->vars[0]->value);
    // get the number of rows
    int num_rows = array->num_items;
    // iterate the rows to get the maximum number of columns
    int num_cols = 1;
    for( i=0; i<num_rows; i++ ){
      // get the array value
      Mem *value = &array->value[i];
      // get the array
      sqlite3_array *row = get_array_from_value(value);
      if( row ){
        // get the number of columns
        int row_num_cols = row->num_items;
        // check if the number of columns is greater than the current maximum
        if( row_num_cols>num_cols ){
          num_cols = row_num_cols;
        }
      }
    }
    // allocate the memory for the result set
    v->nMem = num_cols + 1;  // v->aMem[0] is reserved
    v->aMem = sqlite3DbMallocZero(v->db, sizeof(Mem) * v->nMem);
    if( v->aMem==NULL ){
      return SQLITE_NOMEM;
    }
    // initialize the memory cells
    for(i=0; i<v->nMem; i++){
      sqlite3VdbeMemInit(&v->aMem[i], v->db, MEM_Null);
    }

    // save the source array in the procedure object
    procedure->result_array = array;
    // save the position of the current row
    procedure->current_row = -1;

    // change the next opcode to OP_NextResult
    sqlite3ChangeOpcode(v, POS_NEXT_RESULT, OP_NextResult, 0, POS_RESULT_ROW);

    // it is also called by the OP_NextResult opcode:
    sqlite3VdbeNextResult(v);

  } else {

    // allocate the memory for the result set
    v->nMem = cmd->num_vars + 1;  // v->aMem[0] is reserved
    v->aMem = sqlite3DbMallocZero(v->db, sizeof(Mem) * v->nMem);
    if( v->aMem==0 ){
      return SQLITE_NOMEM;
    }
    // initialize the memory cells
    for(i=0; i<v->nMem; i++){
      sqlite3VdbeMemInit(&v->aMem[i], v->db, MEM_Null);
    }

    // move the values from the variables to the result set
    for( i=0; i<cmd->num_vars; i++ ){
      sqlite3_value *value = &cmd->vars[i]->value;
      // arrays cannot be returned with multiple parameters
      if( is_array(value) ){
        // set the error message
        sqlite3VdbeError(v, "cannot return an array with multiple parameters");
        // return the error code
        return SQLITE_ERROR;
      }
      // copy the value to the result set (aMem[0] is reserved)
      XTRACE("copying value %lld\n", value->u.i);
      sqlite3VdbeMemMove(&v->aMem[i+1], value);
      //sqlite3VdbeMemShallowCopy(&v->aMem[i+1], value, MEM_Static);
      //sqlite3VdbeMemCopy(&v->aMem[i+1], value);
    }

    //v->nResColumn = cmd->num_vars;
    sqlite3VdbeSetNumCols(v, cmd->num_vars);

    // change the result opcode to OP_ResultRow
    sqlite3ChangeOpcode(v, POS_RESULT_ROW, OP_ResultRow, 1, cmd->num_vars);
  }

  return rc;
}

/*
** Execute a RAISE command.
*/
SQLITE_PRIVATE int executeRaiseCommand(Vdbe *v, command *cmd) {
  stored_proc *procedure = cmd->procedure;
  sqlite3 *db = procedure->db;
  char *sql, *msg=NULL;
  int rc = SQLITE_OK;

  // get the SQL statement
  sql = sqlite3_mprintf("SELECT printf(%.*s)", cmd->nsql, cmd->sql);
  if( sql==NULL ){
    return SQLITE_NOMEM;
  }

  // evaluate the expression
  rc = db_query_str(procedure, sql, &msg);
  sqlite3_free(sql);
  if( rc ){
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
    return rc;
  }

  // set the error message
  sqlite3VdbeError(v, "%s", msg);
  sqlite3_free(msg);

  // return the error code
  return rc;
}

/*
** Execute an ASSERT statement.
*/
SQLITE_PRIVATE int executeAssertCommand(Vdbe *v, command *cmd) {
  stored_proc *procedure = cmd->procedure;
  sqlite3 *db = procedure->db;
  char *sql, *msg=NULL;
  int rc = SQLITE_OK;
  bool result;

  // evaluate the condition expression
  rc = execute_expression(v, cmd, &result);
  if (rc != SQLITE_OK) {
    return rc;
  }

  // if the condition is true, we're done
  if (result) {
    return SQLITE_OK;
  }

  // condition failed - evaluate the error message expression
  sql = sqlite3_mprintf("SELECT printf(%.*s)", cmd->nsql2, cmd->sql2);
  if (sql == NULL) {
    return SQLITE_NOMEM;
  }

  // get the formatted error message
  rc = db_query_str(procedure, sql, &msg);
  sqlite3_free(sql);
  if (rc != SQLITE_OK) {
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
    return rc;
  }

  // set the error message
  sqlite3VdbeError(v, "Assertion failed: %s", msg);
  sqlite3_free(msg);

  // return the error code
  return SQLITE_ERROR;
}

/*
** Execute a statement command.
*/
SQLITE_PRIVATE int executeStatementCommand(Vdbe *v, command *cmd) {
  stored_proc *procedure = cmd->procedure;
  sqlite3 *db = procedure->db;
  int rc = SQLITE_OK;

  // reject transaction commands
  if( cmd->sql[0]=='B' || cmd->sql[0]=='C' || cmd->sql[0]=='R' ||
      cmd->sql[0]=='S' ){
    if( (cmd->nsql>=5 && sqlite3_strnicmp(cmd->sql, "BEGIN", 5)==0) ||
        (cmd->nsql>=6 && sqlite3_strnicmp(cmd->sql, "COMMIT", 6)==0) ||
        (cmd->nsql>=8 && sqlite3_strnicmp(cmd->sql, "ROLLBACK", 8)==0) ||
        (cmd->nsql>=8 && sqlite3_strnicmp(cmd->sql, "SAVEPOINT", 8)==0) ||
        (cmd->nsql>=7 && sqlite3_strnicmp(cmd->sql, "RELEASE", 7)==0) ){
      // set the error message
      sqlite3VdbeError(v, "transaction commands are not allowed in stored procedures");
      // return the error code
      return SQLITE_ERROR;
    }
  }else
  if( cmd->sql[0]=='A' || cmd->sql[0]=='D' ){
    if( (cmd->nsql>=6 && sqlite3_strnicmp(cmd->sql, "ATTACH", 6)==0) ||
        (cmd->nsql>=6 && sqlite3_strnicmp(cmd->sql, "DETACH", 6)==0) ){
      // set the error message
      sqlite3VdbeError(v, "attach/detach commands are not allowed in stored procedures");
      // return the error code
      return SQLITE_ERROR;
    }
  }

  // prepare the statement if it is not prepared yet
  if( cmd->stmt==NULL ){
    // parse the SQL statement
    rc = sqlite3_prepare_v2(db, cmd->sql, cmd->nsql, &cmd->stmt, NULL);
    if( rc!=SQLITE_OK ){
      goto loc_error;
    }
  }

  // bind local variable values used on the prepared statement
  if( procedure->vars!=NULL ){
    bindLocalVariables(procedure, cmd->stmt);
  }

  // execute the prepared statement
  do {
    rc = sqlite3_step(cmd->stmt);
  } while (rc == SQLITE_ROW);

  // if the statement was executed successfully, set the return code to SQLITE_OK
  if( rc==SQLITE_DONE ){
    rc = SQLITE_OK;
  }

  // reset the prepared statement
  sqlite3_reset(cmd->stmt);

  // check if there was an error
  if( rc!=SQLITE_OK ){
    goto loc_error;
  }

  return rc;

loc_error:
  // if there was an error, set the error message
  if( v->zErrMsg==NULL ){
    //
  }
  // return the error code
  return rc;
}

/*
** Execute a SET command.
*/
SQLITE_PRIVATE int executeSetCommand(Vdbe *v, command *cmd) {
  stored_proc *procedure = cmd->procedure;
  int rc = SQLITE_OK;

  // similar to the STATEMENT command: execute the prepared statement and store
  // the returned values in the defined variables

  sqlite3_array *parent_array = NULL;
  void (*free_func)(sqlite3_array*) = sqlite3_free_array;
  int num_rows = 0;

  if (cmd->flags & CMD_FLAG_STORE_AS_ARRAY) {
    // exactly one variable to store the array
    if( cmd->num_vars != 1 ){
      sqlite3VdbeError(v, "expected a single variable to store the result");
      rc = SQLITE_ERROR;
      goto loc_exit;
    }
  }

  // is the input an ARRAY?
  if (cmd->input_array != NULL) {
    assert(cmd->stmt == NULL);
    assert(cmd->flags & CMD_FLAG_STORE_AS_ARRAY);
    // get the pointer to the input array
    parent_array = cmd->input_array;
    // do not release it if the variable is set to another value
    free_func = NULL;
    // make the variable point to the input array
    goto loc_set_values;
  }

  // then the input must be a SQL statement

  // check if the prepared statement is available
  if( cmd->stmt == NULL ){
    char *new_sql = NULL;
    char *sql = cmd->sql;
    int nsql = cmd->nsql;
    int n, tokenType;
    /* Read the next token */
    n = sqlite3GetToken((u8*)sql, &tokenType);
    if( n > cmd->nsql ){
      // this should not happen
      sqlite3VdbeError(v, "invalid token: %s", sql);
      rc = SQLITE_ERROR;
      goto loc_exit;
    }
    //keywordCode(sql, n, &tokenType);
    if( tokenType==TK_ID ){
      // check if it is a CALL, then set tokenType to TK_CALL
      if( n==4 && sqlite3_strnicmp(sql, "CALL", 4)==0 ){
        tokenType = TK_CALL;
      }
    }
    switch( tokenType ){
      case TK_SELECT:
      case TK_INSERT:
      case TK_UPDATE:
      case TK_DELETE:
      case TK_CALL:
        // just use the statement as it is
        break;
      default:
        // otherwise, add "SELECT " to the beginning of the statement
        new_sql = sqlite3_mprintf("SELECT %s", sql);
        if( !new_sql ){
          sqlite3VdbeError(v, "out of memory");
          rc = SQLITE_NOMEM;
          goto loc_exit;
        }
        sql = new_sql;
        nsql = strlen(sql);
    }
    // parse the statement
    rc = sqlite3_prepare_v2(procedure->db, sql, nsql, &cmd->stmt, NULL);
    if (new_sql) sqlite3_free(new_sql);
    if (rc != SQLITE_OK) {
      sqlite3VdbeError(v, "error parsing statement: %s", sqlite3_errmsg(procedure->db));
      goto loc_exit;
    }
    if (cmd->stmt == NULL) {
      sqlite3VdbeError(v, "SET command without input");
      rc = SQLITE_ERROR;
      goto loc_exit;
    }

  }else{
    // reset the prepared statement
    sqlite3_reset(cmd->stmt);
  }

  // bind local variable values used on the prepared statement
  if( procedure->vars!=NULL ){
    bindLocalVariables(procedure, cmd->stmt);
  }

  // execute the prepared statement
  do {
    rc = sqlite3_step(cmd->stmt);
    if (rc == SQLITE_ROW) {
      // get the number of columns returned
      int num_cols = sqlite3_column_count(cmd->stmt);
      if (num_cols == 0) {
        sqlite3VdbeError(v, "no columns returned");
        rc = SQLITE_ERROR;
        goto loc_exit;
      }
      // increment the number of rows returned
      num_rows++;
      // if the statement is expected to return many rows, store them on an array variable
      if (cmd->flags & CMD_FLAG_STORE_AS_ARRAY) {

        // allocate an sqlite3_array object with the proper number of values
        sqlite3_array *array = (sqlite3_array*) sqlite3MallocZero(
            sizeof(sqlite3_array) + sizeof(sqlite3_value) * (num_cols-1));
        if (array == NULL) {
          rc = SQLITE_NOMEM;
          goto loc_exit;
        }
        // store the number of items in the array
        array->num_items = num_cols;
        // store the result in the array
        for (int ncol = 0; ncol < num_cols; ncol++) {
          sqlite3_value *array_value = &array->value[ncol];
          sqlite3_value *col_value = sqlite3_column_value(cmd->stmt, ncol);
          sqlite3VdbeMemInit(array_value, procedure->db, MEM_Null);
          sqlite3VdbeMemCopy(array_value, col_value);
        }

        // prepare to store the array in the parent array
        if( parent_array==NULL ){
          // allocate the parent array
          parent_array = (sqlite3_array*) sqlite3MallocZero(sizeof(sqlite3_array));
          if( parent_array==NULL ){
            sqlite3_free(array);
            rc = SQLITE_NOMEM;
            goto loc_exit;
          }
          parent_array->num_items = 1;
        }else{
          // allocate more space
          sqlite3_array *new_array = (sqlite3_array*) sqlite3Realloc(
              parent_array, sizeof(sqlite3_array) +
              (sizeof(sqlite3_value) * parent_array->num_items));
          if( new_array==NULL ){
            sqlite3_free(array);
            rc = SQLITE_NOMEM;
            goto loc_exit;
          }
          parent_array = new_array;
          parent_array->num_items++;
        }
        // store the array in the parent array
        sqlite3_value *value = &parent_array->value[parent_array->num_items-1];
        sqlite3VdbeMemInit(value, procedure->db, MEM_Null);
        sqlite3ValueSetArray(value, array, sqlite3_free_array);

      } else {

        // throw an error if more than one row is returned
        if( num_rows>1 ){
          sqlite3VdbeError(v, "statement returns more than one row");
          rc = SQLITE_ERROR;
          goto loc_exit;
        }

        // check the number of columns returned
        if( num_cols!=cmd->num_vars ){
          sqlite3VdbeError(v, "statement returns %d values but has %d variables to set",
                              num_cols, cmd->num_vars);
          rc = SQLITE_ERROR;
          goto loc_exit;
        }

        // store the result in the defined variables
        for( int ncol=0; ncol<cmd->num_vars; ncol++ ){
          sqlite3_var *var = cmd->vars[ncol];
          sqlite3_value *col_value = sqlite3_column_value(cmd->stmt, ncol);
          sqlite3VdbeMemCopy(&var->value, col_value);
          if( var->type==SQLITE_AFF_REAL ){
            sqlite3_value_numeric_type(&var->value);
          }else if( var->type!=0 && var->type!=SQLITE_AFF_BLOB ){
            sqlite3ValueApplyAffinity(&var->value, var->type, SQLITE_UTF8);  // or ENC(db)
          }
        }

      }

    }
  } while (rc == SQLITE_ROW);

  if (rc == SQLITE_DONE) {
    rc = SQLITE_OK;
  }

  // check if the statement was executed successfully
  if (rc != SQLITE_OK) {
    goto loc_error;
  }

  // reset the prepared statement
  //sqlite3_reset(cmd->stmt);

loc_set_values:

  if (cmd->flags & CMD_FLAG_STORE_AS_ARRAY) {
    // store the parent array in the defined variable
    sqlite3_var *var = cmd->vars[0];
    if (parent_array) {
      sqlite3ValueSetArray(&var->value, parent_array, free_func);
    } else {
      // no row was returned
      sqlite3VdbeMemSetNull(&var->value);
    }
  } else {
    // if there is no returned rows, set the defined variables to NULL
    if (num_rows == 0) {
      for (int nvar = 0; nvar < cmd->num_vars; nvar++) {
        sqlite3_var *var = cmd->vars[nvar];
        sqlite3VdbeMemSetNull(&var->value);
      }
    }
  }

loc_exit:
  // in case of error, release allocated resources
  //! TODO
  return rc;
loc_error:
  if( rc==SQLITE_OK ) rc = SQLITE_ERROR;
  if( v->zErrMsg==NULL ){
  //sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
    sqlite3VdbeError(v, "%s", sqlite3ErrStr(rc));
  }
  goto loc_exit;
}

/*
** Execute a foreach command.
** Retrieve the next item from the array or the next row from the SQL statement
** and save the result in the defined variables.
** If there is a new row, return SQLITE_ROW. Otherwise, return SQLITE_DONE.
**
** Check if already executing using the cmd->current_item variable. Use it as the
** index of the next item to retrieve from the array. For SQL statements, just
** call sqlite3_step() to retrieve the next row and increment the cmd->current_item.
** The input is an array if cmd->input_array is not NULL.
** The input is a SQL statement if cmd->sql is not NULL.
** If the input is a SQL statement, parse it into the cmd->stmt variable if not yet done.
*/
SQLITE_PRIVATE int executeForeachCommand(Vdbe *v, stored_proc *procedure, command *cmd) {
  sqlite3 *db = v->db;
  int rc = SQLITE_OK;
  int num_cols = 0;
  sqlite3_array *input_array = NULL;
  sqlite3_value *row_value = NULL;
  sqlite3_array *row_array = NULL;
  bool has_dynamic_values = false;

  if (cmd->input_var) {
    // retrieve the array from the input variable
    input_array = get_array_from_value(&cmd->input_var->value);
    if (input_array == NULL) {
      sqlite3VdbeError(v, "the input variable %s does not contain an array",
                          cmd->input_var->name);
      goto loc_error;
    }
  } else if (cmd->input_array) {
    input_array = cmd->input_array;
  }

  // retrieve the next item from the array or the next row from the SQL statement
  if (input_array) {
    if (cmd->current_item >= input_array->num_items) {
      // no more items
      cmd->current_item = 0;
      rc = SQLITE_DONE;
      goto loc_exit;
    }
    // retrieve the next item from the array
    row_value = &input_array->value[cmd->current_item];
    // if the row contains an array, retrieve it
    row_array = get_array_from_value(row_value);
    // increment the current item
    cmd->current_item++;
  } else {
    // retrieve the next row from the SQL statement
    if (cmd->current_item == 0) {
      if (cmd->stmt == NULL) {
        // parse the SQL statement
        rc = sqlite3_prepare_v2(db, cmd->sql, cmd->nsql, &cmd->stmt, NULL);
        if (rc != SQLITE_OK) {
          goto loc_error;
        }
      } else {
        // reset the prepared statement
        sqlite3_reset(cmd->stmt);
      }
      // bind the variables
      bindLocalVariables(procedure, cmd->stmt);
    }
    // execute the SQL statement
    rc = sqlite3_step(cmd->stmt);
    if (rc == SQLITE_ROW) {
      // increment the current item
      cmd->current_item++;
    } else if (rc == SQLITE_DONE) {
      // no more rows
      cmd->current_item = 0;
      goto loc_exit;
    } else {
      goto loc_error;
    }
    // if no variables are defined, retrieve them from the SQL statement
    if (cmd->num_vars==0) {
      has_dynamic_values = true;
    }
  }

  // check the number of columns returned
  if (row_array) {
    num_cols = row_array->num_items;
  } else if (row_value) {
    num_cols = 1;
  } else {
    num_cols = sqlite3_column_count(cmd->stmt);
  }
  if (num_cols != cmd->num_vars && has_dynamic_values==false) {
    sqlite3VdbeError(v, "statement returns %d values but has %d variables to set",
        num_cols, cmd->num_vars);
    rc = SQLITE_ERROR;
    goto loc_error;
  }

//! should it allow overwrite the values of existing variables?

//! should the new variables be removed after the foreach command?

  if (has_dynamic_values) {
    // store the result in new variables
    for (int ncol = 0; ncol < num_cols; ncol++) {
      char *col_name = NULL;
      sqlite3_value *col_value;
      sqlite3_var *var;
      col_name = (char *)sqlite3_column_name(cmd->stmt, ncol);
      col_value = sqlite3_column_value(cmd->stmt, ncol);
      // add '@' to the column name
      col_name = sqlite3_mprintf("@%s", col_name);
      if (col_name == NULL) {
        rc = SQLITE_NOMEM;
        goto loc_error;
      }
      // add a new variable
      var = addVariable(procedure, col_name, strlen(col_name), 0, NULL);
      sqlite3_free(col_name);
      if (var == NULL) {
        rc = SQLITE_NOMEM;
        goto loc_error;
      }
      // store the column value in the variable
      sqlite3VdbeMemMove(&var->value, col_value);
    }
  } else {
    // store the result in the defined variables
    for (int ncol = 0; ncol < cmd->num_vars; ncol++) {
      sqlite3_var *var = cmd->vars[ncol];
      sqlite3_value *col_value;
      if (row_array) {
        col_value = &row_array->value[ncol];
        // copy the content from the column to the variable
        sqlite3VdbeMemCopy(&var->value, col_value);
      } else if (row_value) {
        // copy the content from the row to the variable
        sqlite3VdbeMemCopy(&var->value, row_value);
      } else {
        // copy the content from the column to the variable
        col_value = sqlite3_column_value(cmd->stmt, ncol);
        sqlite3VdbeMemMove(&var->value, col_value);
      }
    }
  }

loc_exit:
  return rc;
loc_error:
  if( v->zErrMsg==NULL ){
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
  }
  goto loc_exit;
}

/*
** Execute a stored procedure.
** This function is called by the OP_CallProcedure opcode, on the execute step.
*/
SQLITE_PRIVATE int executeStoredProcedure(Vdbe *v, procedure_call *call) {
  stored_proc *procedure = call->procedure;
  sqlite3 *db = procedure->db;
  char savepoint_name[32];
  char sql[48];
  int rc = SQLITE_OK, rc2;
  int pos, ifpos;
  bool result;
  command *cmd;

  // reset the procedure
  //resetStoredProcedure(v, procedure);  -- already called by sqlite3_reset()

  // reset the OP_ResultRow opcode to OP_Noop
  sqlite3ChangeOpcode(v, POS_RESULT_ROW, OP_Noop, 0, 0);
  // reset the OP_NextResult opcode to OP_Noop
  sqlite3ChangeOpcode(v, POS_NEXT_RESULT, OP_Noop, 0, 0);

  // create a random savepoint name
  do{
    sqlite3_randomness(sizeof(pos), &pos);
  }while (pos<0);
  sqlite3_snprintf(sizeof(savepoint_name), savepoint_name, "sp_%08x", pos);

  // create a savepoint
  sqlite3_snprintf(sizeof(sql), sql, "SAVEPOINT %s", savepoint_name);
  rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
  if (rc != SQLITE_OK) {
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
    return rc;
  }

  // copy from the cmd->input_array to the parameter values (procedure->params[])
  // copy the declared variable values from the v->aVar[] array to the parameter values
  copyProcedureParameters(v, call);

  // iterate and process each command on the stored_proc object
  for (pos = 0; pos < procedure->num_cmds; pos++) {
    cmd = &procedure->cmds[pos];
    switch (cmd->type) {
      case CMD_TYPE_DECLARE:
        // process the DECLARE command
        // if there are default values, store them in the variable
        //! TODO: if the variable was declared with a type, apply the affinity
        // here and on the SET command
        break;
      case CMD_TYPE_RETURN:
        // process the RETURN command
        rc = executeReturnCommand(v, cmd);
        if( rc ) goto loc_error;
        // stop processing the commands
        pos = procedure->num_cmds;

        break;
      case CMD_TYPE_RAISE:
        // process the RAISE command
        rc = executeRaiseCommand(v, cmd);
        if( rc ) goto loc_error;
        // stop processing the commands
        pos = procedure->num_cmds;
        rc = SQLITE_ERROR;

        break;
      case CMD_TYPE_ASSERT:
        // process the ASSERT command
        rc = executeAssertCommand(v, cmd);
        if( rc ) goto loc_error;

        break;
      case CMD_TYPE_SET:
        // process the SET command
        rc = executeSetCommand(v, cmd);
        if( rc ) goto loc_error;

        break;
      case CMD_TYPE_STATEMENT:
        // process the STATEMENT command
        rc = executeStatementCommand(v, cmd);
        if( rc ) goto loc_error;

        break;

      case CMD_TYPE_IF:
        // evaluate the expression
        rc = execute_expression(v, cmd, &result);
        if( rc ) goto loc_error;
        // does the expression evaluate to true?
        if( result ){
          // mark the IF command as executed
          cmd->flags |= CMD_FLAG_EXECUTED;
          // continue execution
        } else {
          // mark the IF command as not executed
          cmd->flags &= ~CMD_FLAG_EXECUTED;
          // skip to the next ELSEIF, ELSE or END IF command
          pos = cmd->next_if_cmd - 1;
        }
        break;
      case CMD_TYPE_ELSEIF:
        // if any previous IF or ELSEIF evaluated to true, skip to the next command
        ifpos = cmd->related_cmd;
        if( procedure->cmds[ifpos].flags & CMD_FLAG_EXECUTED ) {
          // skip to the next command
          pos = cmd->next_if_cmd - 1;
          break;
        }
        // evaluate the expression
        rc = execute_expression(v, cmd, &result);
        if( rc ) goto loc_error;
        // does the expression evaluate to true?
        if( result ){
          // mark the IF block as executed
          ifpos = cmd->related_cmd;
          procedure->cmds[ifpos].flags |= CMD_FLAG_EXECUTED;
          // continue execution
        } else {
          // skip to the next ELSEIF, ELSE or END IF command
          pos = cmd->next_if_cmd - 1;
        }
        break;
      case CMD_TYPE_ELSE:
        // if any previous IF or ELSEIF evaluated to true, skip to the END IF command
        ifpos = cmd->related_cmd;
        if( procedure->cmds[ifpos].flags & CMD_FLAG_EXECUTED ) {
          // skip to the END IF command
          pos = cmd->next_if_cmd - 1;
          break;
        }
        // otherwise, continue execution
        procedure->cmds[ifpos].flags |= CMD_FLAG_EXECUTED;
        break;
      case CMD_TYPE_ENDIF:
        // do nothing
        break;

      case CMD_TYPE_LOOP:
        // do nothing
        break;
      case CMD_TYPE_ENDLOOP:
      case CMD_TYPE_CONTINUE:
        // skip to the LOOP command
        pos = cmd->related_cmd - 1;
        break;
      case CMD_TYPE_BREAK:
        // get the position of the related LOOP command
        pos = cmd->related_cmd;
        cmd = &procedure->cmds[pos];
        // skip over the END LOOP command
        pos = cmd->related_cmd;
        break;

      case CMD_TYPE_FOREACH:
        // process the FOREACH command
        rc = executeForeachCommand(v, procedure, cmd);
        // if it returned a row, continue execution
        if( rc==SQLITE_ROW ) {
          rc = SQLITE_OK;
        // if it returned DONE, skip to the end of the loop
        }else if( rc==SQLITE_DONE ) {
          rc = SQLITE_OK;
          pos = cmd->related_cmd;
        // if it returned an error, stop execution
        }else if( rc ){
          goto loc_error;
        }
        break;
    }
  }

loc_exit:

  // release the savepoint
  sqlite3_snprintf(sizeof(sql), sql, "RELEASE %s", savepoint_name);
  rc2 = sqlite3_exec(db, sql, NULL, NULL, NULL);
  if (rc2 != SQLITE_OK) {
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
    goto loc_error;
  }

  return rc;
loc_error:
  if( v->zErrMsg==NULL ){
    sqlite3VdbeError(v, "%s", sqlite3_errmsg(db));
  }
  XTRACE("execution error (%s): %s\n", command_type_str(cmd->type), v->zErrMsg);
  // rollback to the savepoint
  sqlite3_snprintf(sizeof(sql), sql, "ROLLBACK TO %s", savepoint_name);
  rc2 = sqlite3_exec(db, sql, NULL, NULL, NULL);
  if (rc2 != SQLITE_OK) {
    if (rc == SQLITE_OK) {
      rc = rc2;
    }
  }
  goto loc_exit;
}


////////////////////////////////////////////////////////////////////////////////
// RESET AND RELEASE
////////////////////////////////////////////////////////////////////////////////

/*
** Reset the state of a stored procedure.
*/
SQLITE_PRIVATE int resetStoredProcedure(Vdbe *v, stored_proc* procedure) {
  // if the aMem array was moved to the procedure object
  if( procedure->aMem!=NULL ){
    // release memory from the previous result set
    if( v->aMem ){
      // release each memory cell
      for(int i=0; i<v->nMem; i++){
        sqlite3VdbeMemRelease(&v->aMem[i]);
      }
      // release the memory
      sqlite3DbFree(v->db, v->aMem);
      v->aMem = NULL;
      v->nMem = 0;
    }
    // move the aMem array back to the Vdbe object
    v->aMem = procedure->aMem;
    v->nMem = procedure->nMem;
    // clear the aMem array from the procedure object
    procedure->aMem = NULL;
    procedure->nMem = 0;
  }
  // reset the variables
  sqlite3_var *var;
  for( var=procedure->vars; var; var=var->next ){
    sqlite3VdbeMemSetNull(&var->value);
  }
  return SQLITE_OK;
}

/*
** Reset the state of a procedure call.
*/
SQLITE_PRIVATE int resetProcedureCall(Vdbe *v, procedure_call *call) {
  stored_proc *procedure = call->procedure;
  // reset the stored procedure
  resetStoredProcedure(v, procedure);
  // reset the parameters
  for (int n = 0; n < procedure->num_params; n++) {
    sqlite3_var *param = procedure->params[n];
    sqlite3VdbeMemSetNull(&param->value);
  }
  return SQLITE_OK;
}

/*
** Release a command.
*/
SQLITE_PRIVATE void releaseCommand(command* cmd) {
  if( cmd->flags & CMD_FLAG_DYNAMIC_SQL ){
    sqlite3_free(cmd->sql);
  }
  if (cmd->stmt) {
    sqlite3_finalize(cmd->stmt);
  }
  if (cmd->input_array) {
    sqlite3_free_array(cmd->input_array);
  }
  if (cmd->vars) {
    sqlite3_free(cmd->vars);
  }
}

/*
** Release a stored procedure.
*/
SQLITE_PRIVATE void releaseProcedure(stored_proc* procedure) {
    if (procedure->error_msg) {
        sqlite3_free(procedure->error_msg);
    }
    if (procedure->cmds) {
        for (int n = 0; n < procedure->num_cmds; n++) {
            releaseCommand(&procedure->cmds[n]);
        }
        sqlite3_free(procedure->cmds);
    }
    if (procedure->params) {
        sqlite3_free(procedure->params);
    }
    dropAllVariables(procedure);
    if (procedure->code) {
      sqlite3_free(procedure->code);
    }
    sqlite3_free(procedure);
}

SQLITE_PRIVATE void releaseProcedureCall(procedure_call *call) {
    if (call->procedure) {
        releaseProcedure(call->procedure);
    }
    if (call->input_array) {
        sqlite3_free_array(call->input_array);
    }
    sqlite3_free(call);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

SQLITE_PRIVATE int checkSpecialCommand(Parse *pParse, const char **psql){
    char *sql = (char*) *psql;

    // check if the SQL command is a stored procedure declaration.
    // when it starts with "CREATE [OR REPLACE] [PROCEDURE|FUNCTION]"
    if (sqlite3_strnicmp(sql, "CREATE ", 7) == 0) {
      bool or_replace = false;
      // skip the "CREATE " keyword
      sql += 7;
      // check for the "OR REPLACE " keyword
      if (sqlite3_strnicmp(sql, "OR REPLACE ", 11) == 0) {
        or_replace = true;
        // skip the "OR REPLACE " keyword
        sql += 11;
      }
      // check for the "PROCEDURE " or the "FUNCTION " keywords (on the same line)
      if (sqlite3_strnicmp(sql, "PROCEDURE ", 10) == 0 ||
          sqlite3_strnicmp(sql, "FUNCTION ", 9) == 0) {
        prepareNewStoredProcedure(pParse, &sql, or_replace);
        XTRACE("rc = %d  nErr = %d\n", pParse->rc, pParse->nErr);
        *psql = sql;
        return SQLITE_DONE;
      }
    }

    // check if the SQL command is a call to a stored procedure.
    if (sqlite3_strnicmp(sql, "CALL ", 5) == 0) {
      // skip the "CALL " keyword
      sql += 5;
      prepareProcedureCall(pParse, &sql);
      XTRACE("rc = %d  nErr = %d\n", pParse->rc, pParse->nErr);
      *psql = sql;
      return SQLITE_DONE;
    }

    return SQLITE_OK;
}
