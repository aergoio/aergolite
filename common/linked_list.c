
// correct use:
// llist_add(&first, item);
// llist_prepend(&first, item);
// llist_remove(&first, item);

typedef struct llitem_struct llitem_t;
struct llitem_struct {
    llitem_t *next;
};

SQLITE_PRIVATE void llist_add(void *pfirst, void *pto_add) {
  llitem_t **first, *to_add, *item;

  first = (llitem_t **) pfirst;
  to_add = (llitem_t *) pto_add;

  item = *first;
  if (item == 0) {
    *first = to_add;
  } else {
    while (item->next != 0) {
      item = item->next;
    }
    item->next = to_add;
  }

}

SQLITE_PRIVATE void llist_prepend(void *pfirst, void *pto_add) {
  llitem_t **first, *to_add, *item;

  first = (llitem_t **) pfirst;
  to_add = (llitem_t *) pto_add;

  item = *first;
  *first = to_add;
  to_add->next = item;

}

SQLITE_PRIVATE void llist_remove(void *pfirst, void *pto_del) {
  llitem_t **first, *to_del, *item;

  first = (llitem_t **) pfirst;
  to_del = (llitem_t *) pto_del;

  item = *first;
  if (to_del == item) {
    *first = to_del->next;
  } else {
    while (item != 0) {
      if (item->next == to_del) {
        item->next = to_del->next;
        return;
      }
      item = item->next;
    }
  }

}
