#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#define COPYFROMTO(x1,x2,l1) do { *x2++ = *x1++; } while (x1 < (l1))

/*
#define DEBUG
*/

int compare(char *left, char *right) {
    return strcmp(left, right);
}

int compare2(char **left, char **right) {
    return strcmp(*left, *right);
}

void test_prepare_runs(char **mas, char **aux, int offs, int nelems, int runs) {
    int j = offs;
    int ncnt = 0;
    int nruns = 0;
    int prev = offs;
    while (nruns++ < runs) {
        ncnt += aux[j] - prev;
        j = (int)aux[j];
        prev = j;
    }
    assert(j - offs == nelems);
    assert(ncnt == nelems);
}

int prepare_runs(char **mas, char **aux, int nelems) {
    int i, runs = 0, start = 0, end = 0, sense;
    char *tmpelem;
    for (i = 1; i < nelems; ++i) {
        start = i - 1;
        sense = compare(mas[i-1], mas[i]);
        i++;
        while (i < nelems 
                && compare(mas[i-1], mas[i]) == sense) 
        { ++i; }
        end = i;
        aux[start] = end;
        if (sense > 0) {
            /* rotating */
            while (--end > start) {
                tmpelem = mas[end];
                mas[end] = mas[start];
                mas[start] = tmpelem;
                start++;
            }
        }
        runs++;
        if (i == nelems - 1) {
            aux[nelems - 1] = nelems;
            runs++;
        }
    }
#ifdef DEBUG
    test_prepare_runs(mas, aux, 0, nelems, runs);
#endif
    return runs;
}

void test_fbsearch(char **from, char **to, char **mid, char *elem) {
    if (mid == to) { 
        assert(compare(elem, *(to-1)) >= 0);
    } else
    if (mid == from) {
        assert(compare(elem, *from) >= 0);
    } else {
        assert(compare(elem, *mid) >= 0);
    }
}

void test_merge_results(char **from, char **to, int offset, int nruns) {
    int i;
    int j = offset;
    char **p;
    for (i = 0; i < nruns/2; ++i) {
        if (from[j] > 1) {
            p = to + j + 1;
            while (p < to + (int)from[j]) {
                assert(compare(*(p-1), *p) <= 0);
                p++;
            }
        }
        j = (int)from[j];
    }
}

char **fbsearch(char **from, char **to, char *elem) {
    int res;
    char **mid;
    while (from < to) {
        if (to - from == 1) { return from; }
        mid = from + (to - from) / 2;
        res = compare(elem, *mid);
        if (res == 0) {
            return mid;
        } else if (res < 0) {
            to = mid - 1;
        } else {
            from = mid;
        }
    }
    return from;
}

typedef struct stack_entry_s {
    int level;
    int offset;
    int runs;
} stack_entry_t;

void fast_merge(char **mas, int nelems) {
    int i, j, start, end, ncount;
    int runs, level, offset, iwhich;
    char **f1, **l1, **f2, **l2, **mid;
    char **src, **dst, **out;
    char **aux;
    stack_entry_t stack[60], *stackp, *tmpsp;
    char **dmas[3];
    char *tmp;

    aux = (char*)malloc(nelems * sizeof(char*));
    runs = prepare_runs(mas, aux, nelems);

    dmas[0] = mas;
    dmas[1] = aux;
    dmas[2] = mas;

    stackp = stack;

    stackp->level = 0;
    stackp->offset = 0;
    stackp->runs = runs;

    iwhich = 0;
    while (stackp >= stack) {
        level = stackp->level;
        runs = stackp->runs;
        offset = stackp->offset;
        iwhich = level & 1;

        assert(runs > 1);

        src = dmas[iwhich];
        dst = dmas[iwhich+1];

        j = offset;
        out = dst + offset;
        for (i = 0; i < runs; i += 2) {
            if (i + 1 == runs) {
                break;
            }

            start = j;
            f1 = &src[j];
            j = (int)dst[j];
            f2 = l1 = &src[j];

            j = (int)dst[j];
            l2 = &src[j];
            end = j;

            assert(start < end);

            /*
            while (f1 < l1 && f2 < l2) {
                if (compare(*f1, *f2) <= 0) {
                    mid = fbsearch(f1, l1, *f2);
                    assert(mid >= f1);
#ifdef DEBUG
                    test_fbsearch(f1, l1, mid, *f2);
#endif
                    COPYFROMTO(f1, out, mid);
                } else {
                    mid = fbsearch(f2, l2, *f1);
                    assert(mid >= f2);
#ifdef DEBUG
                    test_fbsearch(f2, l2, mid, *f1);
#endif
                    COPYFROMTO(f2, out, mid);
                }
            }
            */

            while (f1 < l1 && f2 < l2) {
                if (compare(*f1, *f2) <= 0) {
                    *out++ = *f1++;
                } else {
                    *out++ = *f2++;
                }
            }

            if (f1 >= l1) COPYFROMTO(f2, out, l2);
            else COPYFROMTO(f1, out, l1);
            src[start] = (int)end;
        }

#ifdef DEBUG
        test_prepare_runs(dst, src, offset, j - offset, runs / 2);
        test_merge_results(src, dst, offset, runs);
#endif

        /* pushing stack-entry for later use */
        if (runs & 1) {
            stackp->offset = j;
            stackp->runs = 1;
            stackp->level = level;
            stackp++;
            runs--;

            ncount = (int)dst[j];
            for (i = j; i < ncount; ++i) {
                tmp = src[i];
                src[i] = dst[i];
                dst[i] = tmp;
            }

#ifdef DEBUG
            test_prepare_runs(dst, src, j, src[j] - j, 1);
#endif
        }

        /* in the end just merge rest */
        if (runs == 2) {

            /* end of algorithm */
            if (stackp == stack) {
                break;
            }

            stackp->runs = 1;
            stackp->offset = offset;
            stackp->level = level ^ 1;
            tmpsp = stackp - 1;

            ncount = j;
            while (tmpsp >= stack) {
                assert(tmpsp->runs == 1);
                stackp->runs++;
                /* it  needs copy */
                if (tmpsp->level != level) {
                    i = tmpsp->offset;
                    j = (int)dst[i];
                    f1 = dst + i;
                    f2 = src + i;
                    l2 = src + j;
                    COPYFROMTO(f2, f1, l2);
                    /* linkchain set up */
                    src[i] = j;
                }
                assert(ncount == tmpsp->offset);
                ncount = src[ncount];
                tmpsp--;
            }

            stack[0].runs = stackp->runs;
            stack[0].offset = stackp->offset;
            stack[0].level = stackp->level;
            stackp = &stack[0];

#ifdef DEBUG
            test_prepare_runs(dst, src, offset, ncount, stackp->runs);
            test_merge_results(src, dst, stackp->offset, stackp->runs * 2);
#endif

        } else {
            /* preparing for next step */
            stackp->offset = offset;
            stackp->runs = runs / 2;
            stackp->level = level ^ 1;
            /* linkchain is already set up */

#ifdef DEBUG
            test_prepare_runs(dst, src, offset, (int)j, runs / 2);
            test_merge_results(src, dst, offset, runs);
#endif
        }
    }

    /* need to relocate */
    if (dmas[iwhich+1] == aux) {
        src = aux;
        dst = mas;
        COPYFROMTO(src, dst, aux+nelems);
    }

    free(aux);
}

void fillbuf(char *buf, int sz) {
    int i;
    for (i = 0; i < sz - 1; ++i) {
        buf[i] = 'a' + rand() % 26;
    }
    buf[sz-1] = 0;
}

    /*
    char *mas[NCOUNT] = {
        "hello", 
        "hi",   
        "feel", 
        "dragon",
        "smile", 
        "freak",
        "micro",
        "sanity",
    };

    fast_merge(mas, NCOUNT);
    for (i = 0; i < NCOUNT; ++i) {
        printf("%s\n", mas[i]);
    }
    */

struct stack_element {
    int from;
    int to;
    int flag;
};

#define PUSH(from_s, to_s, flag_s) \
    st->from = from_s;  \
    st->to = to_s; \
    st++->flag = flag_s

#define POP(from_s, to_s, flag_s) \
    from_s=(--st)->from; \
    to_s = st->to; \
    flag_s = st->flag

void merge_sort(int from, int to, char **mas, char **temp) {
    if (from >= to)
        return;

    int mid = from + (to - from) / 2;
    merge_sort(from, mid, mas, temp);
    merge_sort(mid + 1, to, mas, temp);

    int i = from;
    int j = mid + 1;
    int c = to - from + 1;
    int k = 0;

    while (k != c) {
        while (i <= mid && j <= to && compare(mas[i], mas[j]) <= 0) 
        { temp[from + k] = mas[i++]; k++; }
        while (i <= mid && j <= to && compare(mas[j], mas[i]) < 0) 
        { temp[from + k] = mas[j++]; k++; }
        while (i > mid && j <= to)
        { temp[from + k] = mas[j++]; k++; }
        while (j > to && i <= mid)
        { temp[from + k] = mas[i++]; k++; }
    }
    memcpy(mas+from, temp+from, (to-from+1) * sizeof(char*));
}

void merge_sort1(int from, int to, char **mas, char **temp) {
    int mid, done = -2, need_pop = 1;
    struct stack_element stack[1000];
    struct stack_element *st = stack;

    PUSH(from, to, -2);

    while (st != stack) {

        if (need_pop) {
            POP(from, to, done);
        }

        if (from >= to) {
            need_pop = 1;
            continue;
        }
    
        mid = from + (to - from) / 2;
        if (done > 0) {
            char **r = temp+from;
            char **f = mas+from; char **f1 = mas+mid+1;
            char **t = mas+mid+1; char **t1 = mas+to+1;
            while (f < f1) {
                while (t >= t1 && f < f1) { *r++ = *f++; break; }
                while (f < f1 && t < t1 && compare(*f, *t) <= 0) { *r++ = *f++; }
                while (f < f1 && t < t1 && compare(*f, *t) > 0) { *r++ = *t++; }
            }
            while (t < t1) { *r++ = *t++; }
            memcpy(mas+from, temp+from, (to-from+1) * sizeof(char *));
            need_pop = 1;

        } else {
            PUSH(from, to, 1);
            PUSH(mid + 1, to, 0);
            to = mid;
            done = 0;
            need_pop = 0;
        }
    }
}

#define REPORT_ELAPSED(stage) do {\
    int64_t ms = (to.tv_usec - from.tv_usec) / 1000;\
    ms += (to.tv_sec - from.tv_sec) * 1000;\
    printf("(%s): %lld\n", (stage), (long long int)ms);\
} while (0)

int main() {
    int i = 0, sz1, sz2;
    int ncount, nelem;
    char **ptrmas, *bigmas, *origbigmas, *origptrmas, **tempptrmas;
    struct timeval from, to;
    char buf[1024];

    int ncounts[9] = { 100, 250, 500, 1000, 5000, 10000, 30000, 100000, 200000 };
    int nelems[6] = { 8, 16, 24, 32, 64, 128 };

    /*
    for (ncount = 0; ncount < sizeof(ncounts)/sizeof(ncounts[0]); ++ncount) {
        for (nelem = 0; nelem < sizeof(nelems)/sizeof(nelems[0]); ++nelem) {
        */
    for (ncount = 8; ncount < 9; ++ncount) {
        for (nelem = 3; nelem < 4; ++nelem) {
            sz1 = ncounts[ncount] * nelems[nelem];
            sz2 = sizeof(char*) * ncounts[ncount];
            bigmas = malloc(sz1);
            ptrmas = malloc(sz2);
            tempptrmas = malloc(sz2);

            origbigmas = malloc(sz1);
            origptrmas = malloc(sz2);
        
            for (i = 0; i < ncounts[ncount]; ++i) {
                ptrmas[i] = &bigmas[i * nelems[nelem]];
                fillbuf(ptrmas[i], nelems[nelem]);
            }

            memcpy(origbigmas, bigmas, sz1);
            memcpy(origptrmas, ptrmas, sz2);

            /*
            gettimeofday(&from, NULL);
            qsort(ptrmas, ncounts[ncount], sizeof(char*), 
                    (int(*)(const void *, const void *))&compare2);
            gettimeofday(&to, NULL);

            sprintf(buf, "qsort(%d/%d)", ncounts[ncount], nelems[nelem]);
            REPORT_ELAPSED(buf);

            memcpy(bigmas, origbigmas, sz1);
            memcpy(ptrmas, origptrmas, sz2);
            */

            /*
            gettimeofday(&from, NULL);
            merge_sort(0, ncounts[ncount]-1, ptrmas, tempptrmas);
            gettimeofday(&to, NULL);

            sprintf(buf, "mergesort(%d/%d)", ncounts[ncount], nelems[nelem]);
            REPORT_ELAPSED(buf);

            memcpy(bigmas, origbigmas, sz1);
            memcpy(ptrmas, origptrmas, sz2);

            */

            gettimeofday(&from, NULL);
            fast_merge(ptrmas, ncounts[ncount]);
            gettimeofday(&to, NULL);

            sprintf(buf, "fast merge(%d/%d)", ncounts[ncount], nelems[nelem]);
            REPORT_ELAPSED(buf);

            memcpy(bigmas, origbigmas, sz1);
            memcpy(ptrmas, origptrmas, sz2);


            /*
            gettimeofday(&from, NULL);
            merge_sort1(0, ncounts[ncount]-1, ptrmas, tempptrmas);
            gettimeofday(&to, NULL);

            sprintf(buf, "mergesort stackless(%d/%d)", 
                    ncounts[ncount], nelems[nelem]);
            REPORT_ELAPSED(buf);

            memcpy(bigmas, origbigmas, sz1);
            memcpy(ptrmas, origptrmas, sz2);
            */

            free(tempptrmas);
            free(origbigmas);
            free(origptrmas);
            free(ptrmas);
            free(bigmas);
        }
    }


    return 0;
}
