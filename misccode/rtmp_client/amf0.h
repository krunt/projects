#ifndef AMF0_DEF_
#define AMF0_DEF_

#include <myclib/all.h>

typedef enum amf0_marker_s {
    ma_amf0_number = 0,
    ma_amf0_boolean,
    ma_amf0_string,
    ma_amf0_object,
    ma_amf0_movieclip,
    ma_amf0_null,
    ma_amf0_undefined,
    ma_amf0_reference,
    ma_amf0_array,
    ma_amf0_object_end,
    ma_amf0_strict_array,
    ma_amf0_date,
    ma_amf0_long_string,
    ma_amf0_unsupported,
    ma_amf0_recordset,
    ma_amf0_xml_document,
    ma_amf0_typed_object,
    ma_amf0_last = ma_amf0_typed_object,
} amf0_marker_t;

typedef struct amf0_value_s {
    amf0_marker_t type;
    union {
        u8 v1;
        double val;
        u64 val64;
        u32 *utf;
        struct list_head props;
    } u;

    union {
        struct {
            int len;      /* in symbols */
            int capacity; /* in symbols */
        } utf;

        int array_len;
    } l;

} amf0_value_t;

typedef struct amf0_object_property_s {
    struct list_head node;
    amf0_value_t *key;
    amf0_value_t *value;
} amf0_object_property_t;

amf0_value_t *amf0_decode_value(u8 **p, u8 *end);
amf0_value_t *amf0_decode_number(u8 **p, u8 *end);
amf0_value_t *amf0_decode_boolean(u8 **p, u8 *end);
amf0_value_t *amf0_decode_string(u8 **p, u8 *end);
amf0_value_t *amf0_decode_string_long(u8 **p, u8 *end);
amf0_value_t *amf0_decode_object(u8 **p, u8 *end);
amf0_value_t *amf0_decode_date(u8 **p, u8 *end);
amf0_value_t *amf0_decode_array(u8 **p, u8 *end);

int amf0_encode_value(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_number(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_boolean(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_string(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_string_long(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_object(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_date(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_array(amf0_value_t *v, u8 **p, u8 *end);
int amf0_encode_undefined(u8 **p, u8 *end);
int amf0_encode_null(u8 **p, u8 *end);
int amf0_encode_object_end(u8 **p, u8 *end);

amf0_value_t *amf0_create_number(double number);
amf0_value_t *amf0_create_boolean(int b);
amf0_value_t *amf0_create_string(const char *p);
amf0_value_t *amf0_create_bstring(const char *p, int len);
amf0_value_t *amf0_create_date(double date);
amf0_value_t *amf0_create_array();
int amf0_array_insert(amf0_value_t *arr, amf0_value_t *k, amf0_value_t *v);
amf0_value_t *amf0_create_object();
int amf0_object_insert(amf0_value_t *obj, amf0_value_t *k, amf0_value_t *v);
int amf0_free(amf0_value_t *v);

#endif /* AMF0_DEF_ */
