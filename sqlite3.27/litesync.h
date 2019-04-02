
#ifndef _LITESYNC_H
#define _LITESYNC_H

/*
** declarations for the litereplica
*/

#include <binn.h>



/* connection type */

//#define NODE_BIND     1    /*  */  <- OR MAYBE THESE
//#define NODE_CONNECT  2    /*  */

#define ADDRESS_BIND     1    /*  */
#define ADDRESS_CONNECT  2    /*  */


/* exported functions */

void litesync_join();


/* replica status */

/* conn_status */

#define CONN_STATUS_DISCONNECTED  0
#define CONN_STATUS_STARTING      1
#define CONN_STATUS_UPDATING      2
#define CONN_STATUS_IN_SYNC       3
#define CONN_STATUS_CONN_LOST     4
#define CONN_STATUS_INVALID_PEER  5  //??????
#define CONN_STATUS_BUSY          6
#define CONN_STATUS_ERROR         7

/* db_state */
#define DB_STATE_UPDATED          2
#define DB_STATE_OUTOFDATE        1
#define DB_STATE_UNKNOWN          0


#endif //_LITESYNC_H
