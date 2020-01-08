
/****************************************************************************/

SQLITE_PRIVATE int random_number(int lower, int upper){
  unsigned int n;
  sqlite3_randomness(sizeof(n), &n);
  return (n % (upper - lower + 1)) + lower;
}

/****************************************************************************/

SQLITE_PRIVATE int calculate_node_wait_interval(plugin *plugin) {

  //random_no, proof = xxxxxxx(plugin, seed);
  //interval = xxxxxxx(random_no, plugin->total_authorized_nodes, plugin->block_interval);

  int lower = plugin->block_interval / 2;
  int upper = plugin->block_interval / 2 * plugin->total_authorized_nodes;
  plugin->random_block_interval = random_number(lower, upper);
  assert( plugin->random_block_interval>=lower && plugin->random_block_interval<=upper );
  //plugin->block_interval_proof = ...
  return plugin->random_block_interval;

}

/****************************************************************************/

SQLITE_PRIVATE int get_block_wait_interval(plugin *plugin){
  return plugin->block_interval / 2;
}
