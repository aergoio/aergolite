
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

void print_nodes(char *title, int list[]){
  printf("%s=%d  { ", title, len_array_list(list));
  for(int i=0; list[i]; i++){
    if( i>0 ) printf(", ");
    printf("%d", list[i]);
  }
  puts(" }");
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

void test_reconnection(
  int n, bool bind_to_random_ports,
  /* nodes that should be disconnected */
  int disconnect_nodes[],
  /* transactions executed on disconnected nodes while in split */
  int num_txns_on_offline_nodes,
  int active_offline_nodes[],
  /* transactions executed on connected nodes while in split */
  int num_txns_on_online_nodes,
  int active_online_nodes[],
  /* transactions executed after the nodes were reconnected and in sync */
  int num_txns_on_reconnect,
  int active_nodes_on_reconnect[]
){
  sqlite3 *db[512];
  char uri[256];
  int rc, i, j, count, done;
  int last_nonce[512];

  printf("----------------------------------------------------------\n"
         "test_reconnection(\n"
         "  nodes=%d\n", n);
  printf("  random_ports=%s\n", bind_to_random_ports ? "yes" : "no");
  print_nodes("  disconnect_nodes", disconnect_nodes);
  print_nodes("  active_offline_nodes", active_offline_nodes);
  print_nodes("  active_online_nodes", active_online_nodes);
  print_nodes("  active_nodes_on_reconnect", active_nodes_on_reconnect);
  printf("  num_txns_on_offline_nodes=%d\n", num_txns_on_offline_nodes);
  printf("  num_txns_on_online_nodes=%d\n", num_txns_on_online_nodes);
  printf("  num_txns_on_reconnect=%d\n", num_txns_on_reconnect);
  puts(")");
  fflush(stdout);

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


  /* set the initial nonce value for each node */

  for(i=1; i<=n; i++){
    last_nonce[i] = 1;
  }

  for(i=1; i<=n; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


  /* execute 3 db transactions on one of the databases */

// (later or in other fn) configurable: if it does this now or after leader election, how many txns, by which node

  db_execute(db[1], "create table t1 (name)");
  db_execute(db[1], "insert into t1 values ('aa1')");
  db_execute(db[1], "insert into t1 values ('aa2')");

  last_nonce[1] = 4;

  db_check_int(db[1], "PRAGMA last_nonce", 4);
  for(i=1; i<=n; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


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

  for(i=1; i<=n; i++){

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

    char sql[128];
    sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[i]);
    db_check_str(db[i], sql, "processed");

  }


  /* disconnect some nodes */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];
    printf("disconnecting node %d\n", node);
    sqlite3_close(db[node]);
    db[node] = 0;
  }


  /* execute transactions on online nodes */

  if( num_txns_on_online_nodes>0 ){
    assert(len_array_list(active_online_nodes)>0);

    puts("executing new transactions on online nodes...");

    for(j=0, i=0; j<num_txns_on_online_nodes; j++, i++){
      int node = active_online_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_online_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('online')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* wait until the transactions are processed in a new block */

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_online_nodes[i]; i++){
      int node = active_online_nodes[i];

      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(150000);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);

    }

    puts("");

    /* check if the data was replicated to the other nodes */

    for(i=1; i<=n; i++){

      if( in_array_list(i,disconnect_nodes) ) continue;

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

      db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

    }

  }


  /* execute transactions on offline nodes */

  if( num_txns_on_offline_nodes>0 ){
    assert(len_array_list(active_offline_nodes)>0);

    /* reopen the nodes in off-line mode */

    for(i=0; active_offline_nodes[i]; i++){
      int node = active_offline_nodes[i];
      printf("reopening node %d in offline mode\n", node);
      sprintf(uri, "file:db%d.db?blockchain=on&num_nodes=%d", node, n);
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }

    puts("executing new transactions on offline nodes...");

    for(j=0, i=0; j<num_txns_on_offline_nodes; j++, i++){
      int node = active_offline_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_offline_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('offline')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);

      //db_check_int(db[node], "select count(*) from t1", 2 + num_txns_on_offline_nodes);
      //db_check_int(db[node], "select count(*) from t1 where name='aa1'", 1);
      //db_check_int(db[node], "select count(*) from t1 where name='aa2'", 1);
      //db_check_int(db[node], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    }

    /* close the off-line nodes */

    for(i=0; active_offline_nodes[i]; i++){
      int node = active_offline_nodes[i];
      sqlite3_close(db[node]);
      db[node] = 0;
    }

  }


  /* reconnect the nodes */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];
    printf("reconnecting node %d\n", node);
    if( node==1 ){
      //assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
      sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[1])==SQLITE_OK );
    }else if( node==2 ){
      //assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
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

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];

    printf("checking node %d\n", node); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      char *result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_str(&result, db[node], "pragma protocol_status");
      assert(rc==SQLITE_OK);
      done = strstr(result,"\"is_leader\": true")>0 || strstr(result,"\"leader\": null")==0;
      sqlite3_free(result);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[node], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    }
    assert(done);

    db_check_int(db[node], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    db_check_int(db[node], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    db_check_int(db[node], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

  }


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n; i++){

    if( in_array_list(i,disconnect_nodes) ) continue;

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

  }


  /* execute new transactions after reconnection */

  if( num_txns_on_reconnect>0 ){
    assert(len_array_list(active_nodes_on_reconnect)>0);

    puts("executing new transactions after reconnection...");

    for(j=0, i=0; j<num_txns_on_reconnect; j++, i++){
      int node = active_nodes_on_reconnect[i];
      if( node==0 ){
        i = 0;
        node = active_nodes_on_reconnect[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('reconnect')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* wait until the transactions are processed in a new block */

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_nodes_on_reconnect[i]; i++){
      int node = active_nodes_on_reconnect[i];

      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(150000);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);

    }

    puts("");

    /* check if the data was replicated to the other nodes */

    for(i=1; i<=n; i++){

      printf("checking node %d\n", i); fflush(stdout);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from t1");
        assert(rc==SQLITE_OK);
        done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes + num_txns_on_reconnect);
      }
      assert(done);

      db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes + num_txns_on_reconnect);
      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='reconnect'", num_txns_on_reconnect);

    }

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
//  test_n_nodes(50, true);
//  test_n_nodes(100, true);

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 3,
    /* active_nodes_on_reconnect[] */ (int[]){3,6,9,0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 5,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 3,
    /* active_offline_nodes[],     */ (int[]){4,10,0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 3,
    /* active_offline_nodes[],     */ (int[]){4,10,0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 5,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,0}
  );

  test_reconnection(25, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,0},
    /* num_txns_on_offline_nodes,  */ 6,
    /* active_offline_nodes[],     */ (int[]){2,7,15,23,0},
    /* num_txns_on_online_nodes,   */ 6,
    /* active_online_nodes[],      */ (int[]){3,8,11,17,0},
    /* num_txns_on_reconnect,      */ 9,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,21,0}
  );

  test_reconnection(50, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

  test_reconnection(100, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

#if 0
  test_reconnection(150, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

  test_reconnection(200, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );
#endif

  /* delete the test db files on success */
  delete_files(200);

  puts("OK. All tests pass!"); return 0;
}
