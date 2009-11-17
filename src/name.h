#include <glib.h>

typedef enum packet_type {
    HELLO = 1,
    GET_ID,
    GET_NAME,
    NAME_ID
} packet_type_t;

typedef struct packet {
    unsigned short sender_id;
    unsigned short type;
    union {
        unsigned short id;
        char name[12];
    } payload;
} __attribute((packed)) packet_t;

