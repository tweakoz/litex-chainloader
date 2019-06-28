#pragma once

#include <stdint.h>

namespace tftp {

    typedef const char* cstr_t;
    size_t get(uint32_t ip, uint16_t server_port, cstr_t filename, void* outbuffer, size_t expected_len );

    constexpr size_t STATUS_FETCH_ERR = 0xffffffff;

} // namespace tftp
