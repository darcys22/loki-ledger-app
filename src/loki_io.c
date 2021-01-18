/*****************************************************************************
 *   Ledger Oxen App.
 *   (c) 2017-2020 Cedric Mesnil <cslashm@gmail.com>, Ledger SAS.
 *   (c) 2020 Ledger SAS.
 *   (c) 2020 Oxen Project
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
#include "cx.h"
#include "oxen_types.h"
#include "oxen_api.h"
#include "oxen_vars.h"

#if defined(IODUMMYCRYPT)
#warning IODUMMYCRYPT activated
#endif
#if defined(IONOCRYPT)
#warning IONOCRYPT activated
#endif

/*
 * io_buff: contains current message part
 * io_off: offset in current message part
 * io_length: length of current message part
 */

/* ----------------------------------------------------------------------- */
/* MISC                                                                    */
/* ----------------------------------------------------------------------- */
void monero_io_set_offset(unsigned int offset) {
    if (offset == IO_OFFSET_END) {
        G_oxen_state.io_offset = G_oxen_state.io_length;
    } else if (offset < G_oxen_state.io_length) {
        G_oxen_state.io_offset = offset;
    } else {
        THROW(ERROR_IO_OFFSET);
    }
}

void monero_io_inserted(unsigned int len) {
    G_oxen_state.io_offset += len;
    G_oxen_state.io_length += len;
}

void monero_io_discard(int clear) {
    G_oxen_state.io_length = 0;
    G_oxen_state.io_offset = 0;
    if (clear) {
        monero_io_clear();
    }
}

void monero_io_clear(void) { os_memset(G_oxen_state.io_buffer, 0, MONERO_IO_BUFFER_LENGTH); }

/* ----------------------------------------------------------------------- */
/* INSERT data to be sent                                                  */
/* ----------------------------------------------------------------------- */

void monero_io_hole(unsigned int sz) {
    if ((G_oxen_state.io_length + sz) > MONERO_IO_BUFFER_LENGTH) {
        THROW(ERROR_IO_FULL);
    }
    os_memmove(G_oxen_state.io_buffer + G_oxen_state.io_offset + sz,
               G_oxen_state.io_buffer + G_oxen_state.io_offset,
               G_oxen_state.io_length - G_oxen_state.io_offset);
    G_oxen_state.io_length += sz;
}

void monero_io_insert(unsigned char const* buff, unsigned int len) {
    monero_io_hole(len);
    os_memmove(G_oxen_state.io_buffer + G_oxen_state.io_offset, buff, len);
    G_oxen_state.io_offset += len;
}

void monero_io_insert_hmac_for(unsigned char* buffer, int len, int type) {
    // for now, only 32bytes block are allowed
    if (len != 32) {
        THROW(SW_WRONG_DATA);
    }

    unsigned char hmac[32 + 1 + 4];

    os_memmove(hmac, buffer, 32);
    hmac[32] = type;
    if (type == TYPE_ALPHA) {
        hmac[33] = (G_oxen_state.tx_sign_cnt >> 0) & 0xFF;
        hmac[34] = (G_oxen_state.tx_sign_cnt >> 8) & 0xFF;
        hmac[35] = (G_oxen_state.tx_sign_cnt >> 16) & 0xFF;
        hmac[36] = (G_oxen_state.tx_sign_cnt >> 24) & 0xFF;
    } else {
        hmac[33] = 0;
        hmac[34] = 0;
        hmac[35] = 0;
        hmac[36] = 0;
    }
    cx_hmac_sha256(G_oxen_state.hmac_key, 32, hmac, 37, hmac, 32);
    monero_io_insert(hmac, 32);
}

void monero_io_insert_encrypt(unsigned char* buffer, int len, int type) {
    // for now, only 32bytes block are allowed
    if (len != 32) {
        THROW(SW_WRONG_DATA);
    }

    monero_io_hole(len);

#if defined(IODUMMYCRYPT)
    for (int i = 0; i < len; i++) {
        G_oxen_state.io_buffer[G_oxen_state.io_offset + i] = buffer[i] ^ 0x55;
    }
#elif defined(IONOCRYPT)
    os_memmove(G_oxen_state.io_buffer + G_oxen_state.io_offset, buffer, len);
#else
    cx_aes(&G_oxen_state.spk, CX_ENCRYPT | CX_CHAIN_CBC | CX_LAST | CX_PAD_NONE, buffer, len,
           G_oxen_state.io_buffer + G_oxen_state.io_offset, len);
#endif
    G_oxen_state.io_offset += len;
    if (G_oxen_state.tx_in_progress) {
        monero_io_insert_hmac_for(G_oxen_state.io_buffer + G_oxen_state.io_offset - len, len,
                                  type);
    }
}

void monero_io_insert_u32(unsigned int v32) {
    monero_io_hole(4);
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] = v32 >> 24;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] = v32 >> 16;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 2] = v32 >> 8;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 3] = v32 >> 0;
    G_oxen_state.io_offset += 4;
}

void monero_io_insert_u24(unsigned int v24) {
    monero_io_hole(3);
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] = v24 >> 16;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] = v24 >> 8;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 2] = v24 >> 0;
    G_oxen_state.io_offset += 3;
}

void monero_io_insert_u16(unsigned int v16) {
    monero_io_hole(2);
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] = v16 >> 8;
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] = v16 >> 0;
    G_oxen_state.io_offset += 2;
}

void monero_io_insert_u8(unsigned int v8) {
    monero_io_hole(1);
    G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] = v8;
    G_oxen_state.io_offset += 1;
}

/* ----------------------------------------------------------------------- */
/* FECTH data from received buffer                                         */
/* ----------------------------------------------------------------------- */
int monero_io_fetch_available(void) { return G_oxen_state.io_length - G_oxen_state.io_offset; }
void monero_io_assert_available(int sz) {
    if ((G_oxen_state.io_length - G_oxen_state.io_offset) < sz) {
        THROW(SW_WRONG_LENGTH + (sz & 0xFF));
    }
}

int monero_io_fetch(unsigned char* buffer, int len) {
    monero_io_assert_available(len);
    if (buffer) {
        os_memmove(buffer, G_oxen_state.io_buffer + G_oxen_state.io_offset, len);
    }
    G_oxen_state.io_offset += len;
    return len;
}

static void monero_io_verify_hmac_for(const unsigned char* buffer, int len,
                                      unsigned char* expected_hmac, int type) {
    // for now, only 32bytes block allowed
    if (len != 32) {
        THROW(SW_WRONG_DATA);
    }

    unsigned char hmac[37];
    os_memmove(hmac, buffer, 32);
    hmac[32] = type;
    if (type == TYPE_ALPHA) {
        hmac[33] = (G_oxen_state.tx_sign_cnt >> 0) & 0xFF;
        hmac[34] = (G_oxen_state.tx_sign_cnt >> 8) & 0xFF;
        hmac[35] = (G_oxen_state.tx_sign_cnt >> 16) & 0xFF;
        hmac[36] = (G_oxen_state.tx_sign_cnt >> 24) & 0xFF;
    } else {
        hmac[33] = 0;
        hmac[34] = 0;
        hmac[35] = 0;
        hmac[36] = 0;
    }
    cx_hmac_sha256(G_oxen_state.hmac_key, 32, hmac, 37, hmac, 32);
    if (os_memcmp(hmac, expected_hmac, 32)) {
        monero_lock_and_throw(SW_SECURITY_HMAC);
    }
}

int monero_io_fetch_decrypt(unsigned char* buffer, int len, int type) {
    // for now, only 32bytes block allowed
    if (len != 32) {
        THROW(SW_WRONG_LENGTH);
    }

    if (G_oxen_state.tx_in_progress) {
        monero_io_assert_available(len + 32);
        monero_io_verify_hmac_for(G_oxen_state.io_buffer + G_oxen_state.io_offset, len,
                                  G_oxen_state.io_buffer + G_oxen_state.io_offset + len,
                                  type);
    } else {
        monero_io_assert_available(len);
    }

    if (buffer) {
#if defined(IODUMMYCRYPT)
        for (int i = 0; i < len; i++) {
            buffer[i] = G_oxen_state.io_buffer[G_oxen_state.io_offset + i] ^ 0x55;
        }
#elif defined(IONOCRYPT)
        os_memmove(buffer, G_oxen_state.io_buffer + G_oxen_state.io_offset, len);
#else  // IOCRYPT
        cx_aes(&G_oxen_state.spk, CX_DECRYPT | CX_CHAIN_CBC | CX_LAST | CX_PAD_NONE,
               G_oxen_state.io_buffer + G_oxen_state.io_offset, len, buffer, len);
#endif
    }
    G_oxen_state.io_offset += len;
    if (G_oxen_state.tx_in_progress) {
        G_oxen_state.io_offset += 32;
    }
    if (buffer) {
        switch (type) {
            case TYPE_SCALAR:
                monero_check_scalar_range_1N(buffer);
                break;
            case TYPE_AMOUNT_KEY:
            case TYPE_DERIVATION:
            case TYPE_ALPHA:
                monero_check_scalar_not_null(buffer);
                break;
            default:
                THROW(SW_SECURITY_INTERNAL);
        }
    }
    return len;
}

int monero_io_fetch_decrypt_key(unsigned char* buffer) {
    unsigned char* k;
    monero_io_assert_available(32);

    k = G_oxen_state.io_buffer + G_oxen_state.io_offset;
    // view?
    if (os_memcmp(k, C_FAKE_SEC_VIEW_KEY, 32) == 0) {
        G_oxen_state.io_offset += 32;
        if (G_oxen_state.tx_in_progress) {
            monero_io_assert_available(32);
            monero_io_verify_hmac_for(C_FAKE_SEC_VIEW_KEY, 32,
                                      G_oxen_state.io_buffer + G_oxen_state.io_offset,
                                      TYPE_SCALAR);
            G_oxen_state.io_offset += 32;
        }
        os_memmove(buffer, G_oxen_state.view_priv, 32);
        return 32;
    }
    // spend?
    else if (os_memcmp(k, C_FAKE_SEC_SPEND_KEY, 32) == 0) {
        switch (G_oxen_state.io_ins) {
            case INS_VERIFY_KEY:
            case INS_DERIVE_SECRET_KEY:
                // case INS_GET_SUBADDRESS_SPEND_PUBLIC_KEY:
                break;
            default:
                THROW(SW_WRONG_DATA);
        }
        G_oxen_state.io_offset += 32;
        if (G_oxen_state.tx_in_progress) {
            monero_io_assert_available(32);
            monero_io_verify_hmac_for(C_FAKE_SEC_SPEND_KEY, 32,
                                      G_oxen_state.io_buffer + G_oxen_state.io_offset,
                                      TYPE_SCALAR);
        }
        os_memmove(buffer, G_oxen_state.spend_priv, 32);
        return 32;
    }
    // else
    else {
        return monero_io_fetch_decrypt(buffer, 32, TYPE_SCALAR);
    }
}

uint64_t monero_io_fetch_varint(void) {
    uint64_t v64;
    G_oxen_state.io_offset +=
        monero_decode_varint(G_oxen_state.io_buffer + G_oxen_state.io_offset,
                             MIN(10, G_oxen_state.io_length - G_oxen_state.io_offset), &v64);
    return v64;
}

uint32_t monero_io_fetch_varint32(void) {
    uint64_t v64 = monero_io_fetch_varint();
    if (v64 > 0xffffffffULL) THROW(SW_WRONG_DATA_RANGE);
    return (uint32_t) v64;
}

uint16_t monero_io_fetch_varint16(void) {
    uint64_t v64 = monero_io_fetch_varint();
    if (v64 > 0xffffULL) THROW(SW_WRONG_DATA_RANGE);
    return (uint16_t) v64;
}

unsigned int monero_io_fetch_u32(void) {
    unsigned int v32;
    monero_io_assert_available(4);
    v32 = ((G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] << 24) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] << 16) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 2] << 8) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 3] << 0));
    G_oxen_state.io_offset += 4;
    return v32;
}

unsigned int monero_io_fetch_u24(void) {
    unsigned int v24;
    monero_io_assert_available(3);
    v24 = ((G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] << 16) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] << 8) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 2] << 0));
    G_oxen_state.io_offset += 3;
    return v24;
}

unsigned int monero_io_fetch_u16(void) {
    unsigned int v16;
    monero_io_assert_available(2);
    v16 = ((G_oxen_state.io_buffer[G_oxen_state.io_offset + 0] << 8) |
           (G_oxen_state.io_buffer[G_oxen_state.io_offset + 1] << 0));
    G_oxen_state.io_offset += 2;
    return v16;
}

unsigned int monero_io_fetch_u8(void) {
    unsigned int v8;
    monero_io_assert_available(1);
    v8 = G_oxen_state.io_buffer[G_oxen_state.io_offset];
    G_oxen_state.io_offset += 1;
    return v8;
}

/* ----------------------------------------------------------------------- */
/* REAL IO                                                                 */
/* ----------------------------------------------------------------------- */

int monero_io_do(unsigned int io_flags) {
    // if IO_ASYNCH_REPLY has been  set,
    //  io_exchange will return when  IO_RETURN_AFTER_TX will set in ui
    if (io_flags & IO_ASYNCH_REPLY) {
        io_exchange(CHANNEL_APDU | IO_ASYNCH_REPLY, 0);
    }
    // else send data now
    else {
        G_oxen_state.io_offset = 0;
        if (G_oxen_state.io_length > MONERO_APDU_LENGTH) {
            THROW(SW_IO_FULL);
        }
        os_memmove(G_io_apdu_buffer, G_oxen_state.io_buffer + G_oxen_state.io_offset,
                   G_oxen_state.io_length);

        if (io_flags & IO_RETURN_AFTER_TX) {
            io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, G_oxen_state.io_length);
            return 0;
        } else {
            io_exchange(CHANNEL_APDU, G_oxen_state.io_length);
        }
    }

    //--- set up received data  ---
    G_oxen_state.io_offset = 0;
    G_oxen_state.io_length = 0;
    G_oxen_state.io_protocol_version = G_io_apdu_buffer[0];
    G_oxen_state.io_ins = G_io_apdu_buffer[1];
    G_oxen_state.io_p1 = G_io_apdu_buffer[2];
    G_oxen_state.io_p2 = G_io_apdu_buffer[3];
    G_oxen_state.io_lc = 0;
    G_oxen_state.io_le = 0;
    G_oxen_state.io_lc = G_io_apdu_buffer[4];
    os_memmove(G_oxen_state.io_buffer, G_io_apdu_buffer + 5, G_oxen_state.io_lc);
    G_oxen_state.io_length = G_oxen_state.io_lc;

    return 0;
}
