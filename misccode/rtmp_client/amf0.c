
#include "amf0.h"
#include <assert.h>
#include <string.h>

static amf0_value_t *alloc_value(amf0_marker_t type) {
    amf0_value_t *res = (amf0_value_t *)malloc(sizeof(amf0_value_t));
    res->type = type;
    return res;
}

amf0_value_t *amf0_decode_value(u8 **p, u8 *end) {
    int type = **p; (*p)++;
    switch (type) {
    case ma_amf0_number: return amf0_decode_number(p, end);
    case ma_amf0_boolean: return amf0_decode_boolean(p, end);
    case ma_amf0_string: return amf0_decode_string(p, end);
    case ma_amf0_object: return amf0_decode_object(p, end);
    case ma_amf0_movieclip: goto unsupported;
    case ma_amf0_null: return alloc_value(ma_amf0_null);
    case ma_amf0_undefined: return alloc_value(ma_amf0_undefined);
    case ma_amf0_reference: goto unsupported;
    case ma_amf0_array: return amf0_decode_array(p, end);
    case ma_amf0_object_end: goto unsupported;
    case ma_amf0_strict_array: return amf0_decode_array(p, end);
    case ma_amf0_date: return amf0_decode_date(p, end);
    case ma_amf0_long_string: return amf0_decode_string_long(p, end);
    case ma_amf0_unsupported: goto unsupported;
    case ma_amf0_recordset: goto unsupported;
    case ma_amf0_xml_document: goto unsupported;
    case ma_amf0_typed_object: 
    default: goto unsupported;
    };

unsupported:
    assert(0);
}

#define NEED_SIZE(n)  \
    if (end - *p < n)  \
        return NULL;  

amf0_value_t *amf0_decode_number(u8 **p, u8 *end) {
    amf0_value_t *v;
    NEED_SIZE(8);
    v = alloc_value(ma_amf0_number);
    unpack8_be(&v->u.val, *p);
    *p += 8;
    return v;
}

amf0_value_t *amf0_decode_boolean(u8 **p, u8 *end) {
    amf0_value_t *v;
    NEED_SIZE(1);
    v = alloc_value(ma_amf0_boolean);
    unpack1_be(&v->u.v1, *p);
    (*p)++;
    return v;
}

static amf0_value_t *amf0_decode_string_common(u8 **p, u8 *end, int m) {
    int i;
    u32 len;
    u32 symb;
    u8 *pb;
    amf0_value_t *v;

    NEED_SIZE(m);
    if (m == 2) {
        unpack2_be(&len, *p);
    } else {
        unpack4_be(&len, *p);
    }
    *p += m;

    NEED_SIZE(len);

    v = alloc_value(ma_amf0_string);

    v->l.utf.capacity = 8;
    v->l.utf.len = 0;
    v->u.utf = malloc(v->l.utf.capacity * sizeof(u32));

    for (i = 0, pb = *p; pb < *p + len; ++i) {
        if (utf_decode(&symb, &pb, *p + len - pb))
            return NULL;

        if (i > v->l.utf.capacity) {
            v->l.utf.capacity *= 2;
            v->u.utf = realloc(v->u.utf, v->l.utf.capacity * sizeof(u32));
        }

        v->u.utf[i] = symb;
        v->l.utf.len++;
    }

    *p += len;
    return v;
}

amf0_value_t *amf0_decode_string(u8 **p, u8 *end) {
    return amf0_decode_string_common(p, end, 2);
}

amf0_value_t *amf0_decode_string_long(u8 **p, u8 *end) {
    return amf0_decode_string_common(p, end, 4);
}

amf0_value_t *amf0_decode_object(u8 **p, u8 *end) {
    amf0_value_t *v, *key, *value;
    amf0_object_property_t *prop;

    v = alloc_value(ma_amf0_object);

    INIT_LIST_HEAD(&v->u.props);

    while (*p < end) {
        prop = malloc(sizeof(amf0_object_property_t));
        key = amf0_decode_string(p, end);

        if (!key)
            break;

        if (key->l.utf.len == 0) {
            if (*p < end && **p == 0x09) {
                (*p)++;
                break;
            }
        }

        value = amf0_decode_value(p, end);

        if (!value)
            break;

        prop->key = key;
        prop->value = value;

        INIT_LIST_HEAD(&prop->node);
        list_add(&prop->node, &v->u.props);
    }

    return v;
}

amf0_value_t *amf0_decode_date(u8 **p, u8 *end) {
    amf0_value_t *v;
    NEED_SIZE(8 + 2);
    v = alloc_value(ma_amf0_number);
    unpack8_be(&v->u.val, *p);
    *p += 8 + 2;
    return v;
}

amf0_value_t *amf0_decode_array(u8 **p, u8 *end) {
    int i;
    amf0_value_t *v, *key, *value;
    amf0_object_property_t *prop;

    NEED_SIZE(4);
    unpack4_be(&v->l.array_len, *p);
    *p += 4;

    v = alloc_value(ma_amf0_array);

    INIT_LIST_HEAD(&v->u.props);

    for (i = 0; i < v->l.array_len; ++i) {

        prop = malloc(sizeof(amf0_object_property_t));
        key = amf0_decode_string(p, end);

        if (!key)
            break;

        value = amf0_decode_value(p, end);

        if (!value)
            break;

        prop->key = key;
        prop->value = value;

        INIT_LIST_HEAD(&prop->node);
        list_add(&prop->node, &v->u.props);
    }

    return v;
}

int amf0_encode_value(amf0_value_t *v, u8 **p, u8 *end) {
    u8 type = v->type;
    switch (type) {
    case ma_amf0_number: return amf0_encode_number(v, p, end);
    case ma_amf0_boolean: return amf0_encode_boolean(v, p, end);
    case ma_amf0_string: return amf0_encode_string(v, p, end);
    case ma_amf0_object: return amf0_encode_object(v, p, end);
    case ma_amf0_movieclip: goto unsupported;
    case ma_amf0_null: return amf0_encode_null(p, end);
    case ma_amf0_undefined: return amf0_encode_undefined(p, end);
    case ma_amf0_reference: goto unsupported;
    case ma_amf0_array: return amf0_encode_array(v, p, end);
    case ma_amf0_object_end: return amf0_encode_object_end(p, end);
    case ma_amf0_strict_array: goto unsupported;
    case ma_amf0_date: return amf0_encode_date(v, p, end);
    case ma_amf0_long_string: return amf0_encode_string(v, p, end);
    case ma_amf0_unsupported: goto unsupported;
    case ma_amf0_recordset: goto unsupported;
    case ma_amf0_xml_document: goto unsupported;
    case ma_amf0_typed_object: 
    default: goto unsupported;
    };

    return 0;

unsupported:
    assert(0);
}

#undef NEED_SIZE
#define NEED_SIZE(n)  \
    if (end - *p < n)  \
        return 1;  

int amf0_encode_undefined(u8 **p, u8 *end) {
    u8 type = ma_amf0_undefined;
    NEED_SIZE(1);
    pack1_be(*p, &type); (*p)++;
    return 0;
}

int amf0_encode_null(u8 **p, u8 *end) {
    u8 type = ma_amf0_null;
    NEED_SIZE(1);
    pack1_be(*p, &type); (*p)++;
    return 0;
}

int amf0_encode_boolean(amf0_value_t *v, u8 **p, u8 *end) {
    u8 type = ma_amf0_boolean;
    NEED_SIZE(2);
    pack1_be(*p, &type); (*p)++;
    pack1_be(*p, &v->u.v1); (*p)++;
    return 0;
}

int amf0_encode_number(amf0_value_t *v, u8 **p, u8 *end) {
    u8 type = ma_amf0_number;
    NEED_SIZE(9);
    pack1_be(*p, &type); (*p)++;
    pack8_be(*p, &v->u.val); *p += 8;
    return 0;
}

static int amf0_encode_string_common(amf0_value_t *v, u8 **p, u8 *end, 
        int m, int with_type) 
{
    int i;
    u8 type = m == 4 ? ma_amf0_long_string : ma_amf0_string;
    u32 len = v->l.utf.len;
    NEED_SIZE((with_type ? 1 : 0) + m);

    if (with_type) {
        pack1_be(*p, &type); (*p)++;
    }

    if (m == 4) {
        pack4_be(*p, &len); *p += m;
    } else {
        pack2_be(*p, &len); *p += m;
    }

    for (i = 0; i < len; ++i) {
        if (utf_encode(v->u.utf[i], p, end - *p))
            return 1;
    }
    return 0;
}

int amf0_encode_string(amf0_value_t *v, u8 **p, u8 *end) {
    return amf0_encode_string_common(v, p, end, 2, 1);
}

int amf0_encode_string_long(amf0_value_t *v, u8 **p, u8 *end) {
    return amf0_encode_string_common(v, p, end, 4, 1);
}

int amf0_encode_string_wotype(amf0_value_t *v, u8 **p, u8 *end) {
    return amf0_encode_string_common(v, p, end, 2, 0);
}

int amf0_encode_object(amf0_value_t *v, u8 **p, u8 *end) {
    amf0_object_property_t *prop;
    u8 type = ma_amf0_object;
    NEED_SIZE(1);
    pack1_be(*p, &type); (*p)++;
    list_for_each_entry(prop, &v->u.props, node) { 
        if (amf0_encode_string_wotype(prop->key, p, end)
            || amf0_encode_value(prop->value, p, end))
            return 1;
    }
    if (amf0_encode_object_end(p, end))
        return 1;
    return 0;
}

int amf0_encode_date(amf0_value_t *v, u8 **p, u8 *end) {
    u8 type = ma_amf0_date;
    NEED_SIZE(9);
    pack1_be(*p, &type); (*p)++;
    pack8_be(*p, &v->u.val); *p += 8;
    **p = 0; (*p)++; /* time-zone (2 bytes) */
    **p = 0; (*p)++;
    return 0;
}

int amf0_encode_object_end(u8 **p, u8 *end) {
    NEED_SIZE(3);
    **p = 0; (*p)++;
    **p = 0; (*p)++;
    **p = 9; (*p)++;
    return 0;
}

int amf0_encode_array(amf0_value_t *v, u8 **p, u8 *end) {
    amf0_object_property_t *prop;
    u32 len = v->l.array_len;
    u8 type = ma_amf0_array;
    NEED_SIZE(5);
    pack1_be(*p, &type); (*p)++;
    pack4_be(*p, &len); *p += 4;
    list_for_each_entry(prop, &v->u.props, node) {
        if (amf0_encode_value(prop->key, p, end)
            || amf0_encode_value(prop->value, p, end))
            return 1;
    }
    return 0;
}

amf0_value_t *amf0_create_number(double number) {
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_number;
    v->u.val = number;
    return v;
}

amf0_value_t *amf0_create_boolean(int b) {
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_boolean;
    v->u.v1 = b ? 1 : 0;
    return v;
}

amf0_value_t *amf0_create_string(const char *p) {
    int i, len;
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    len = strlen(p);
    v->type = ma_amf0_string;
    v->l.utf.len = len;
    v->u.utf = malloc(len * sizeof(u32));
    for (i = 0; i < len; ++i) {
        v->u.utf[i] = (u8)p[i];
    }
    return v;
}

amf0_value_t *amf0_create_bstring(const char *p, int len) {
    int i;
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_string;
    v->l.utf.len = len;
    v->u.utf = malloc(len * sizeof(u32));
    for (i = 0; i < len; ++i) {
        v->u.utf[i] = p[i];
    }
    return v;
}

amf0_value_t *amf0_create_date(double date) {
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_date;
    v->u.val = date;
    return v;
}

amf0_value_t *amf0_create_array() {
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_array;
    INIT_LIST_HEAD(&v->u.props);
    v->l.array_len = 0;
    return v;
}

int  amf0_array_insert(amf0_value_t *arr, amf0_value_t *k, amf0_value_t *v) {
    amf0_object_property_t *prop;
    prop = malloc(sizeof(amf0_object_property_t));
    INIT_LIST_HEAD(&prop->node);
    prop->key = k;
    prop->value = v;
    list_add(&prop->node, &arr->u.props);
    return 0;
}

amf0_value_t *amf0_create_object() {
    amf0_value_t *v = malloc(sizeof(amf0_value_t));
    v->type = ma_amf0_object;
    INIT_LIST_HEAD(&v->u.props);
    return v;
}

int  amf0_object_insert(amf0_value_t *obj, amf0_value_t *k, amf0_value_t *v) {
    amf0_object_property_t *prop;
    prop = malloc(sizeof(amf0_object_property_t));
    INIT_LIST_HEAD(&prop->node);
    prop->key = k;
    prop->value = v;
    list_add(&prop->node, &obj->u.props);
    return 0;
}

int amf0_free(amf0_value_t *v) {
    return 0;
}
