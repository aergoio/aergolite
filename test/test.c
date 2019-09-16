
struct product {
  char   name[32];
  int    qty;
  double price;
};

typedef struct product product;

#include "db_functions.c"

const int wait_time = 150000;

/****************************************************************************/

void delete_files(int n){
  int i;
  for(i=1; i<=n; i++){
    unlinkf("db%d.db", i);
    unlinkf("db%d.db-loc", i);
    unlinkf("db%d.db-con", i);
    unlinkf("db%d.db-shm", i);
    unlinkf("db%d.db-state", i);
    unlinkf("db%d.db-state-wal", i);
    unlinkf("db%d.db-state-shm", i);
  }
}

/****************************************************************************/
/****************************************************************************/

void test_5_nodes(int bind_to_random_ports){
  sqlite3 *db1, *db2, *db3, *db4, *db5;
  int rc, count, done;

  printf("test_5_nodes(random_ports=%d)...", bind_to_random_ports); fflush(stdout);

  /* delete the db files if they exist */

  delete_files(5);

  /* open the connections to the databases */

  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db1)==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db2)==SQLITE_OK );

  if( bind_to_random_ports ){
  assert( sqlite3_open("file:db3.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db3)==SQLITE_OK );
  assert( sqlite3_open("file:db4.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db4)==SQLITE_OK );
  assert( sqlite3_open("file:db5.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db5)==SQLITE_OK );
  }else{
  assert( sqlite3_open("file:db3.db?blockchain=on&bind=4303&discovery=127.0.0.1:4301,127.0.0.1:4302", &db3)==SQLITE_OK );
  assert( sqlite3_open("file:db4.db?blockchain=on&bind=4304&discovery=127.0.0.1:4301,127.0.0.1:4302", &db4)==SQLITE_OK );
  assert( sqlite3_open("file:db5.db?blockchain=on&bind=4305&discovery=127.0.0.1:4301,127.0.0.1:4302", &db5)==SQLITE_OK );
  }


  /* execute 3 db transactions on one of the databases */

  db_check_int(db1, "PRAGMA last_nonce", 1);

  db_execute(db1, "create table t1 (name)");
  db_execute(db1, "insert into t1 values ('aa1')");
  db_execute(db1, "insert into t1 values ('aa2')");

  db_check_int(db1, "PRAGMA last_nonce", 4);
  db_check_int(db2, "PRAGMA last_nonce", 1);
  db_check_int(db3, "PRAGMA last_nonce", 1);


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time); // 100 ms
    rc = db_query_str(&result, db1, "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  printf("1"); fflush(stdout);


  /* db2 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db2, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db2, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db2, "select count(*) from t1", 2);
  db_check_int(db2, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db2, "select count(*) from t1 where name='aa2'", 1);

  printf("2"); fflush(stdout);


  /* db3 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db3, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db3, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db3, "select count(*) from t1", 2);
  db_check_int(db3, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db3, "select count(*) from t1 where name='aa2'", 1);

  printf("3"); fflush(stdout);


  /* db4 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db4, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db4, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db4, "select count(*) from t1", 2);
  db_check_int(db4, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db4, "select count(*) from t1 where name='aa2'", 1);

  printf("4"); fflush(stdout);


  /* db5 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db5, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db5, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db5, "select count(*) from t1", 2);
  db_check_int(db5, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db5, "select count(*) from t1 where name='aa2'", 1);

  printf("5"); fflush(stdout);


  /* db1 */

  db_check_int(db1, "select count(*) from t1", 2);
  db_check_int(db1, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db1, "select count(*) from t1 where name='aa2'", 1);


  sqlite3_close(db1);
  sqlite3_close(db2);
  sqlite3_close(db3);
  sqlite3_close(db4);
  sqlite3_close(db5);

  puts(" done");

}

/****************************************************************************/

void test_n_nodes(int n, bool bind_to_random_ports){
  sqlite3 *db[512];
  char uri[256];
  int rc, i, count, done;

  printf("test_n_nodes(nodes=%d, random_ports=%d)...", n, bind_to_random_ports); fflush(stdout);

  assert(n>2 && n<512);

  /* delete the db files if they exist */

  delete_files(n);

  /* open the connections to the databases */

#if 0
  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif

  sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[1])==SQLITE_OK );

  sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[2])==SQLITE_OK );

  for(i=3; i<=n; i++){
    if( bind_to_random_ports ){
      sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, n);
    }else{
      sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, 4300 + i, n);
    }
    //puts(uri);
    assert( sqlite3_open(uri, &db[i])==SQLITE_OK );
  }


  /* execute 3 db transactions on one of the databases */

  db_check_int(db[1], "PRAGMA last_nonce", 1);

  db_execute(db[1], "create table t1 (name)");
  db_execute(db[1], "insert into t1 values ('aa1')");
  db_execute(db[1], "insert into t1 values ('aa2')");

  db_check_int(db[1], "PRAGMA last_nonce", 4);
  db_check_int(db[2], "PRAGMA last_nonce", 1);
  db_check_int(db[3], "PRAGMA last_nonce", 1);


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time);
    rc = db_query_str(&result, db[1], "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=2; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>1);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);

  }


  /* db1 */

  db_check_int(db[1], "select count(*) from t1", 2);
  db_check_int(db[1], "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db[1], "select count(*) from t1 where name='aa2'", 1);



  /* execute more transactions on separate databases */

  puts("inserting more data...");

  db_execute(db[4], "insert into t1 values ('aa3')");
  db_execute(db[3], "insert into t1 values ('aa4')");
  db_execute(db[5], "insert into t1 values ('aa5')");
  db_execute(db[2], "create table t2 (name)");

  db_check_int(db[4], "PRAGMA last_nonce", 2);
  db_check_int(db[3], "PRAGMA last_nonce", 2);
  db_check_int(db[5], "PRAGMA last_nonce", 2);
  db_check_int(db[2], "PRAGMA last_nonce", 2);


  /* wait until the transactions are processed in a new block */

  for(i=2; i<=5; i++){

    printf("waiting new block on node %d", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      char *result;
      usleep(wait_time);
      rc = db_query_str(&result, db[i], "PRAGMA transaction_status(2)");
      assert(rc==SQLITE_OK);
      done = (strcmp(result,"processed")==0);
      sqlite3_free(result);
      printf("."); fflush(stdout);
    }
    assert(done);

    puts("");

  }


  /* check if the data was replicated on all the nodes */

  for(i=1; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t2'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>2);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 5);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa3'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa4'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa5'", 1);

  }


  for(i=1; i<=n; i++){
    sqlite3_close(db[i]);
  }

  puts("done");

}

/****************************************************************************/

void test_reconnection(int n, bool bind_to_random_ports, int len, int list[]){
  sqlite3 *db[512];
  char uri[256];
  int rc, i, count, done;

  printf("test_reconnection(nodes=%d, disconnect=%d, random_ports=%d)...", n, len, bind_to_random_ports); fflush(stdout);

  assert(n>=5 && n<512);

  /* delete the db files if they exist */

  delete_files(n);

  /* open the connections to the databases */

#if 0
  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif

  sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[1])==SQLITE_OK );

  sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[2])==SQLITE_OK );

  for(i=3; i<=n; i++){
    if( bind_to_random_ports ){
      sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, n);
    }else{
      sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, 4300 + i, n);
    }
    //puts(uri);
    assert( sqlite3_open(uri, &db[i])==SQLITE_OK );
  }


  /* execute 3 db transactions on one of the databases */

  db_check_int(db[1], "PRAGMA last_nonce", 1);

  db_execute(db[1], "create table t1 (name)");
  db_execute(db[1], "insert into t1 values ('aa1')");
  db_execute(db[1], "insert into t1 values ('aa2')");

  db_check_int(db[1], "PRAGMA last_nonce", 4);
  db_check_int(db[2], "PRAGMA last_nonce", 1);
  db_check_int(db[3], "PRAGMA last_nonce", 1);


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time); // 100 ms
    rc = db_query_str(&result, db[1], "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=2; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>1);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);

  }


  /* db1 */

  db_check_int(db[1], "select count(*) from t1", 2);
  db_check_int(db[1], "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db[1], "select count(*) from t1 where name='aa2'", 1);


  /* disconnect some nodes */

  for(i=0; i<len; i++){
    int node = list[i];
    printf("disconnecting node %d\n", node);
    sqlite3_close(db[node]);
  }


  /* insert some data */

  puts("executing new transaction...");

  db_execute(db[3], "insert into t1 values ('aa3')");
  db_check_int(db[3], "PRAGMA last_nonce", 2);


  /* wait until the transactions are processed in a new block */

  printf("waiting for new block"); fflush(stdout);

  done = 0;
  for(count=0; !done && count<200; count++){
    char *result;
    usleep(150000);
    rc = db_query_str(&result, db[3], "PRAGMA transaction_status(2)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n; i++){

    if( in_array(i,len,list) ) continue;

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>2);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 3);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa3'", 1);

  }


  /* reconnect the nodes */

  for(int i=0; i<len; i++){
    int node = list[i];
    printf("reconnecting node %d\n", node);
    if( node==1 ){
#if 0
      assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
#endif
      sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[1])==SQLITE_OK );
    }else if( node==2 ){
#if 0
      assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif
      sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[2])==SQLITE_OK );
    }else{
      if( bind_to_random_ports ){
        sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, n);
      }else{
        sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, 4300 + node, n);
      }
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }
  }


  /* check if they are up-to-date */

  for(int j=0; j<len; j++){
    int i = list[j];

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>2);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 3);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa3'", 1);

  }


  /* close the db connections */

  for(i=1; i<=n; i++){
    sqlite3_close(db[i]);
  }

  puts("done");

}

/****************************************************************************/
/****************************************************************************/

int main(){

//  test_5_nodes(0);
//  test_5_nodes(1);

//  test_n_nodes(10, true);
//  test_n_nodes(25, false);
  test_n_nodes(50, true);
//  test_n_nodes(100, true);

  test_reconnection(10, false, 3, (int[]){2,4,10});
//  test_reconnection(25, false, 7, (int[]){2,4,10,15,18,21,24});
  test_reconnection(50, false, 7, (int[]){2,4,10,15,18,21,24});

  puts("OK. All tests pass!"); return 0;
}
