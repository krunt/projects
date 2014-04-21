
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <stdint.h>

#include "all.h"

typedef struct item_s {
    struct list_head node;
    struct rb_node rbnode;
    struct hlist_node hnode;
    int val;
} item_t;

static item_t *get_item(int val) {
    item_t *result = malloc(sizeof(item_t));
    INIT_LIST_HEAD(&result->node);
    INIT_HLIST_NODE(&result->hnode);
    result->val = val;
    return result;
}

static void put_item(item_t *item) {
    free(item);
}

static int list_length(struct list_head *hd) {
    item_t *item;
    int cnt;

    cnt = 0;
    list_for_each_entry(item, hd, node) {
        cnt++;
    }

    return cnt;
}

static int list_sum(struct list_head *hd) {
    item_t *item;
    int sum;

    sum = 0;
    list_for_each_entry(item, hd, node) {
        sum += item->val;
    }

    return sum;
}

void test_list() {
    int cnt;
    struct list_head hd, *old_prev;
    struct item_s *pitem, *item, *tmp;

    INIT_LIST_HEAD(&hd);

    list_add(&get_item(1)->node, &hd);

    assert(list_length(&hd) == 1);

    item = list_first_entry(&hd, item_t, node);
    list_del(hd.next);
    put_item(item);

    assert(list_empty(&hd));
    assert(list_length(&hd) == 0);

    list_add(&get_item(2)->node, &hd);
    list_add(&get_item(3)->node, &hd);
    list_add(&get_item(5)->node, &hd);

    assert(list_sum(&hd) == 10);

    cnt = 0;
    list_for_each_entry_safe(item, tmp, &hd, node) {
        list_del(&item->node);
        put_item(item);
        cnt++;
    }

    assert(cnt == 3);

    cnt = 0;
    list_for_each_entry(item, &hd, node) {
        cnt++;
    }

    assert(cnt == 0);

    list_add(&get_item(5)->node, &hd);

    assert(list_length(&hd) == 1);

    list_del(hd.next);

    assert(list_length(&hd) == 0);

    list_add_tail(&get_item(5)->node, &hd);
    list_add_tail(&get_item(1)->node, &hd);
    list_add(&get_item(4)->node, &hd);

    cnt = 0;
    list_for_each_entry(item, &hd, node) {
        if (++cnt == 1) {
            assert(item->val == 4);
            break;
        }
    }

    cnt = 0;
    list_for_each_entry(item, &hd, node) {
        if (++cnt == 2) {
            assert(item->val == 5);
            break;
        }
    }

    cnt = 0;
    list_for_each_entry(item, &hd, node) {
        if (++cnt == 3) {
            assert(item->val == 1);
            break;
        }
    }
}

void test_time() {
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    usleep(1000);
    gettimeofday(&tv2, NULL);
    assert(time_after(&tv2, &tv1));
}

static struct rb_node *rb_search_item(struct rb_root *root, int val) {
    item_t *item;
    struct rb_node *n = root->rb_node;

    while (n) {
        item = rb_entry(n, item_t, rbnode);
        if (val < item->val)
            n = n->rb_left;
        else if (val > item->val)
            n = n->rb_right;
        else
            return n;
    }
    
    return NULL;
}

static item_t *__rb_insert_item(struct rb_root *root, item_t *t) {
    item_t *item;
    struct rb_node **p = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*p) {
        item = rb_entry(*p, item_t, rbnode);
        parent = *p;
        if (t->val < item->val) {
            p = &(*p)->rb_left;
        }
        else if (t->val > item->val) {
            p = &(*p)->rb_right;
        }
        else
            return item;
    }

    rb_link_node(&t->rbnode, parent, p);
    
    return NULL;
}

static item_t *rb_insert_item(struct rb_root *root, item_t *t) {
    item_t *ret, *item;
    if ((ret = __rb_insert_item(root, t)))
        goto out;
    rb_insert_color(&t->rbnode, root);
out:
    return ret;
}

static int rb_length(struct rb_root *root) {
    int c = 0;
    struct rb_node *node;

    for (node = rb_first(root); node != NULL; node = rb_next(node)) {
        c++;
    }

    return c;
}

void test_rbtree() {
    struct rb_root root = RB_ROOT;

    rb_insert_item(&root, get_item(6));
    rb_insert_item(&root, get_item(4));
    rb_insert_item(&root, get_item(8));

    assert(rb_length(&root) == 3);
    rb_erase(rb_first(&root), &root);
    assert(rb_length(&root) == 2);
    rb_erase(rb_first(&root), &root);
    assert(rb_length(&root) == 1);
    rb_erase(rb_first(&root), &root);
    assert(rb_length(&root) == 0);
}

typedef struct hashtable_s {
    struct hlist_head heads[33];
} hashtable_t;

void htable_init(hashtable_t *t) {
    int i;
    for (i = 0; i < 33; ++i) {
        INIT_HLIST_HEAD(&t->heads[i]);
    }
}

void htable_insert(hashtable_t *t, item_t *item) {
    unsigned int hval = bobj_hash((char*)item, sizeof(item));
    hval %= 33;
    hlist_add_head(&item->hnode, &t->heads[hval]);
}

int htable_count(hashtable_t *t) {
    int i, cnt;
    struct hlist_node *node;

    cnt = 0;
    for (i = 0; i < 33; ++i) {
        hlist_for_each(node, &t->heads[i]) {
            cnt++;
        }
    }

    return cnt;
}

void test_htable() {
    int i;
    hashtable_t table;
    htable_init(&table);

    for (i = 0; i < 100; ++i) {
        htable_insert(&table, get_item(i));
    }
    assert(htable_count(&table) == 100);
}

void test_serialize() {
    char buf[128], *p;

    uint8_t a = 0xFE, a1;
    uint16_t b = 0xBFEF, b1;
    uint32_t c = 0xDDEDDEED, c1;
    uint64_t d = 0xEDDEEDDEAFAF, d1;

    p = buf;
    pack1(p, &a); p++;
    pack2(p, &b); p+=2;
    pack4(p, &c); p+=4;
    pack8(p, &d); p+=8;

    p = buf;
    unpack1(&a1, p); p++;
    unpack2(&b1, p); p+=2;
    unpack4(&c1, p); p+=4;
    unpack8(&d1, p); p+=8;

    assert(a == a1);
    assert(b == b1);
    assert(c == c1);
    assert(d == d1);
}

int main() {
    test_list();
    test_time();
    test_rbtree();
    test_htable();
    test_serialize();
    return 0;
}
