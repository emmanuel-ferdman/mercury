/*
 * match.h
 *
 * Copyright (c) 2019 Cisco Systems, Inc. All rights reserved.
 * License at https://github.com/cisco/mercury/blob/master/LICENSE
 */

#ifndef MATCH_H
#define MATCH_H

#include <stdint.h>
#include <stdlib.h>

unsigned int uint16_match(uint16_t x,
                          const uint16_t *ulist,
                          unsigned int num);

unsigned int u32_compare_masked_data_to_value(const void *data,
                                              const void *mask,
                                              const void *value);

unsigned int u64_compare_masked_data_to_value(const void *data,
                                              const void *mask,
                                              const void *value);

template <size_t N>
class mask_and_value {
    uint8_t mask[N];
    uint8_t value[N];
public:
    constexpr mask_and_value(std::array<uint8_t, N> m, std::array<uint8_t, N> v) : mask{}, value{} {
        for (size_t i=0; i<N; i++) {
            mask[i] = m[i];
            value[i] = v[i];
        }
    }

    bool matches(const uint8_t tcp_data[N]) const {
        return u32_compare_masked_data_to_value(tcp_data, mask, value);  // TODO: verify that N=8
    }

    constexpr size_t length() const { return N; }

    static unsigned int u32_compare_masked_data_to_value(const void *data_in,
                                                         const void *mask_in,
                                                         const void *value_in) {
        const uint32_t *d = (const uint32_t *)data_in;
        const uint32_t *m = (const uint32_t *)mask_in;
        const uint32_t *v = (const uint32_t *)value_in;

        return ((d[0] & m[0]) == v[0]) && ((d[1] & m[1]) == v[1]);
    }

};


#endif /* MATCH_H */
