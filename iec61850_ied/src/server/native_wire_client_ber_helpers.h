#ifndef UNITLAB_NATIVE_WIRE_CLIENT_BER_HELPERS_H
#define UNITLAB_NATIVE_WIRE_CLIENT_BER_HELPERS_H

#include <stddef.h>
#include <stdint.h>

#include "wire/ber/unitlab_mms_ber.h"

int unitlab_native_client_bytes_are_printable_ascii(const uint8_t* bytes, size_t length);

int unitlab_native_client_decode_object_name_domain_item(
    const UnitLabMmsBerElement* object_name,
    char* domain,
    size_t domain_size,
    char* item,
    size_t item_size);

#endif
