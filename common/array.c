

// stores on the allocated buffer:
//  size of item       - uint16
//  num of alloc items - uint16
//  num of used items  - uint16
//  unused             - uint16 - so the content is 64bit aligned

/*

  void *array = new_array(8, sizeof(struct mod_pages));
  if( !array ) return SQLITE_NOMEM;

  // increases the array size automatically if needed
  pos = array_insert_sorted(&array, item1, compare);
  pos = array_insert_sorted(&array, item2, compare);

  count = array_count(array);
  pages = array_ptr(array);
  for( i=0; i<count; i++ ){
    xx = pages[i].pgno;
  }

*/


#define ARRAY_INVALID  -1
#define ARRAY_NOMEM    -2
#define ARRAY_EXISTS   -3


SQLITE_PRIVATE void * new_array(int num_items, int item_size){
  void *array;
  int total_size;
  uint16_t *pshort;

  if( item_size<=0 || num_items<0 ) return NULL;

  total_size = (num_items * item_size) + 8;

  array = sqlite3_malloc_zero(total_size);
  if( !array ) return NULL;

  pshort = (uint16_t *) array;
  pshort[0] = item_size;
  pshort[1] = num_items;  /* allocated items */
  pshort[2] = 0;          /* used items */

  return array;
}

SQLITE_PRIVATE void array_free(void **parray){
  void *array = *parray;
  sqlite3_free(array);
  *parray = NULL;
}

SQLITE_PRIVATE void * array_copy(void *array){
  uint16_t *pshort = (uint16_t *) array;
  int item_size, alloc_items, total_size;
  void *copy;

  if( !array ) return NULL;

  item_size   = pshort[0];
  alloc_items = pshort[1];

  total_size = (alloc_items * item_size) + 8;

  copy = sqlite3_malloc(total_size);
  if( copy ){
    memcpy(copy, array, total_size);
  }
  return copy;
}

SQLITE_PRIVATE int array_count(void *array){
  uint16_t *pshort = (uint16_t *) array;
  if( !array ) return -1;
  return pshort[2];       /* used items */
}

SQLITE_PRIVATE void * array_ptr(void *array){
  uint16_t *pshort = (uint16_t *) array;
  if( !array ) return NULL;
  return &pshort[4];
}

SQLITE_PRIVATE void * array_get(void *array, int pos){
  uint16_t *pshort = (uint16_t *) array;
  int item_size, used_items;
  char *base;

  if( !array ) return NULL;

  item_size   = pshort[0];
  used_items  = pshort[2];

  if( pos<0 || pos>=used_items ) return NULL;

  base = (char*) &pshort[4];  /* array base */

  /* return the item address */
  return base + (pos * item_size);
}

/* increases the array size automatically if needed */
static int array_insert_ex(void **parray, void *item, int pos, int(*compare_fn)(void*,void*), int replace){
  void *array = *parray;
  uint16_t *pshort = (uint16_t *) array;
  char *base, *new_item = (char *) item;
  int i, item_size, alloc_items, used_items;

  if( !array || !item ) return ARRAY_INVALID;

  item_size   = pshort[0];
  alloc_items = pshort[1];  /* allocated items */
  used_items  = pshort[2];  /* used items */

  if( used_items==alloc_items || pos>=alloc_items ){
    if( pos>=alloc_items ) alloc_items = pos + 1;
    else alloc_items *= 2;
    {
      int new_size = (alloc_items * item_size) + 8;
      void *array2 = sqlite3_realloc(array, new_size);
      if( !array2 ) return ARRAY_NOMEM;
      /* update the vars with the new re-allocated buffer */
      array = array2;
    }
    *parray = array;
    pshort = (uint16_t *) array;
    pshort[1] = alloc_items;
  }

  base = (char*) &pshort[4];  /* array base */

  if( compare_fn ){
    for( i=0; i<used_items; i++ ){
      int ret;
      item = base + (i * item_size);
      ret = compare_fn(item, new_item);
      if( ret==0 ){
        if( replace ){
          memcpy(item, new_item, item_size);
          return i;  /* returns the position of the existing item */
        }else{
          return ARRAY_EXISTS;
        }
      // }else if( ret>0 ){
      //   continue to find the position in which the new item should be inserted
      }else if( ret<0 ){
        break;
      }
    }
    /* update the item address */
    item = base + (i * item_size);
  }else{
    if( pos<0 ){  /* append */
      i = used_items;
    }else{        /* prepend or at given position */
      i = pos;
    }
    item = base + (i * item_size);
  }

  if( i<used_items ){
    /* move the existing items to release the space */
    int count = used_items - i;
    char *item2 = base + ((i+1) * item_size);
    memmove(item2, item, count * item_size);
    used_items++;
  }else if( i>used_items ){
    /* clear the unused items */
    int count = i - used_items;
    char *item2 = base + (used_items * item_size);
    memset(item2, 0, count * item_size);
    used_items = pos + 1;
  }else{
    used_items++;
  }
  memcpy(item, new_item, item_size);
  /* update the number of used items */
  pshort[2] = used_items;
  /* return the position of the existing item */
  return i;

}

SQLITE_PRIVATE int array_insert_sorted(void **parray, void *item, int(*compare_fn)(void*,void*), int replace){
  if( !compare_fn ) return ARRAY_INVALID;
  return array_insert_ex(parray, item, 0, compare_fn, replace);
}

SQLITE_PRIVATE int array_insert_at(void **parray, void *item, int pos){
  if( pos<0 ) return ARRAY_INVALID;
  return array_insert_ex(parray, item, pos, NULL, 0);
}

SQLITE_PRIVATE int array_prepend(void **parray, void *item){
  return array_insert_ex(parray, item, 0, NULL, 0);
}

SQLITE_PRIVATE int array_append(void **parray, void *item){
  return array_insert_ex(parray, item, -1, NULL, 0);
}
