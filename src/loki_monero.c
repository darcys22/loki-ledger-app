/*****************************************************************************
 *   Ledger Monero App.
 *   (c) 2017-2020 Cedric Mesnil <cslashm@gmail.com>, Ledger SAS.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include "os.h"
#include "loki_types.h"
#include "loki_api.h"
#include "loki_vars.h"

#ifndef LOKI_ALPHA
const unsigned char C_MAINNET_NETWORK_ID[] = {0x46 ,0x61, 0x72, 0x62 ,0x61, 0x75, 0x74, 0x69,
                                              0x2a, 0x4c, 0x61, 0x75, 0x66, 0x65, 0x79, 0x00};
#endif
const unsigned char C_TESTNET_NETWORK_ID[] = {0x5f, 0x3a, 0x78, 0x65, 0xe1, 0x6f, 0xca, 0xb8,
                                              0x02, 0xa1, 0xdc, 0x17, 0x61, 0x64, 0x15, 0xbe};
const unsigned char C_STAGENET_NETWORK_ID[] = {0xbb ,0x37, 0x9B, 0x22 , 0x0A, 0x66 , 0x69, 0x1E,
                                               0x09, 0xB2, 0x97, 0x8A, 0xCC, 0xA1, 0xDF, 0x9C};

// Copyright (c) 2014-2017, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

const char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
#define alphabet_size (sizeof(alphabet) - 1)
const unsigned int encoded_block_sizes[] = {0, 2, 3, 5, 6, 7, 9, 10, 11};
#define FULL_BLOCK_SIZE         8  //(sizeof(encoded_block_sizes) / sizeof(encoded_block_sizes[0]) - 1)
#define FULL_ENCODED_BLOCK_SIZE 11  // encoded_block_sizes[full_block_size];
#define ADDR_CHECKSUM_SIZE      4

static uint64_t uint_8be_to_64(const unsigned char* data, size_t size) {
    uint64_t res = 0;
    switch (9 - size) {
        case 1:
            res |= *data++;
        case 2:
            res <<= 8;
            res |= *data++;
        case 3:
            res <<= 8;
            res |= *data++;
        case 4:
            res <<= 8;
            res |= *data++;
        case 5:
            res <<= 8;
            res |= *data++;
        case 6:
            res <<= 8;
            res |= *data++;
        case 7:
            res <<= 8;
            res |= *data++;
        case 8:
            res <<= 8;
            res |= *data;
            break;
    }

    return res;
}

static void encode_block(const unsigned char* block, unsigned int size, char* res) {
    uint64_t num = uint_8be_to_64(block, size);
    int i = encoded_block_sizes[size];
    while (i--) {
        uint64_t remainder = num % alphabet_size;
        num /= alphabet_size;
        res[i] = alphabet[remainder];
    }
}

int monero_base58_public_key(char* str_b58, unsigned char* view, unsigned char* spend,
                             unsigned char is_subbadress, unsigned char* paymanetID) {
    unsigned char data[72 + 8];
    unsigned int offset;
    unsigned int prefix;

    // data[0] = N_monero_pstate->network_id;
    switch (N_monero_pstate->network_id) {
        case TESTNET:
            if (paymanetID) {
                prefix = TESTNET_CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
            } else if (is_subbadress) {
                prefix = TESTNET_CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
            } else {
                prefix = TESTNET_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
            }
            break;
        case STAGENET:
            if (paymanetID) {
                prefix = STAGENET_CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
            } else if (is_subbadress) {
                prefix = STAGENET_CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
            } else {
                prefix = STAGENET_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
            }
            break;
#ifndef LOKI_ALPHA
        case MAINNET:
            if (paymanetID) {
                prefix = MAINNET_CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
            } else if (is_subbadress) {
                prefix = MAINNET_CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
            } else {
                prefix = MAINNET_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
            }
            break;
#endif
    }
    offset = monero_encode_varint(data, 8, prefix);

    os_memmove(data + offset, spend, 32);
    os_memmove(data + offset + 32, view, 32);
    offset += 64;
    if (paymanetID) {
        os_memmove(data + offset, paymanetID, 8);
        offset += 8;
    }
    monero_keccak_F(data, offset, G_monero_vstate.mlsagH);
    os_memmove(data + offset, G_monero_vstate.mlsagH, 4);
    offset += 4;

    unsigned int full_block_count = (offset) / FULL_BLOCK_SIZE;
    unsigned int last_block_size = (offset) % FULL_BLOCK_SIZE;
    for (size_t i = 0; i < full_block_count; ++i) {
        encode_block(data + i * FULL_BLOCK_SIZE, FULL_BLOCK_SIZE,
                     &str_b58[i * FULL_ENCODED_BLOCK_SIZE]);
    }

    if (0 < last_block_size) {
        encode_block(data + full_block_count * FULL_BLOCK_SIZE, last_block_size,
                     &str_b58[full_block_count * FULL_ENCODED_BLOCK_SIZE]);
    }

    return 0;
}