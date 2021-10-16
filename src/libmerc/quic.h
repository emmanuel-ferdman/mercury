/*
 * quic.h
 *
 * Copyright (c) 2020 Cisco Systems, Inc. All rights reserved.
 * License at https://github.com/cisco/mercury/blob/master/LICENSE
 */

/**
 * \file quic.h
 *
 * \brief interface file for QUIC code
 */
#ifndef QUIC_H
#define QUIC_H

#include <string>
#include <unordered_map>
#include <arpa/inet.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "json_object.h"
#include "util_obj.h"

/*
 * QUIC header format (from draft-ietf-quic-transport-32):
 *
 *    Long Header Packet {
 *       Header Form (1) = 1,
 *       Fixed Bit (1) = 1,
 *       Long Packet Type (2),
 *       Type-Specific Bits (4),
 *       Version (32),
 *       Destination Connection ID Length (8),
 *       Destination Connection ID (0..160),
 *       Source Connection ID Length (8),
 *       Source Connection ID (0..160),
 *    }
 *
 *    Short Header Packet {
 *       Header Form (1) = 0,
 *       Fixed Bit (1) = 1,
 *       Spin Bit (1),
 *       Reserved Bits (2),
 *       Key Phase (1),
 *       Packet Number Length (2),
 *       Destination Connection ID (0..160),
 *       Packet Number (8..32),
 *       Packet Payload (..),
 *    }
 *
 */

struct uint8_bitfield {
    uint8_t value;

    uint8_bitfield(uint8_t x) : value{x} {}

    void operator()(struct buffer_stream &b) {
        for (uint8_t x = 0x80; x > 0; x=x>>1) {
            if (x & value) {
                b.write_char('1');
            } else {
                b.write_char('0');
            }
        }
    }
};


// parse_variable_length_integer() implements the QUIC variable-length
// integer encoding (following RFC9000, Section 16).  If there is a
// parse error, i.e. the datum being parsed is too short, then the datum
// will be set to NULL state
//
//          +======+========+=============+=======================+
//          | 2MSB | Length | Usable Bits | Range                 |
//          +======+========+=============+=======================+
//          | 00   | 1      | 6           | 0-63                  |
//          +------+--------+-------------+-----------------------+
//          | 01   | 2      | 14          | 0-16383               |
//          +------+--------+-------------+-----------------------+
//          | 10   | 4      | 30          | 0-1073741823          |
//          +------+--------+-------------+-----------------------+
//          | 11   | 8      | 62          | 0-4611686018427387903 |
//          +------+--------+-------------+-----------------------+
//
static ssize_t parse_variable_length_integer(datum &d) {
    uint8_t b;
    d.read_uint8(&b);
    int len=0;
    switch (b & 0xc0) {
    case 0xc0:
        len = 8;
        break;
    case 0x80:
        len = 4;
        break;
    case 0x40:
        len = 2;
        break;
    case 0x00:
        len = 1;
    }
    ssize_t value = (b & 0x3f);
    for (int i=1; i<len; i++) {
        value *= 256;
        d.read_uint8(&b);
        value += (b & 0x3f);
    }
    if (d.is_null()) {
        value = -1;    // perhaps should just leave at 0
    }
    return value;
}

//   Initial Packet {
//     Header Form (1) = 1,
//     Fixed Bit (1) = 1,
//     Long Packet Type (2) = 0,
//     Reserved Bits (2),
//     Packet Number Length (2),
//     Version (32),
//     Destination Connection ID Length (8),
//     Destination Connection ID (0..160),
//     Source Connection ID Length (8),
//     Source Connection ID (0..160),
//     Token Length (i),
//     Token (..),
//     Length (i),
//     Packet Number (8..32),
//     Packet Payload (8..),
//   }
//
struct quic_initial_packet {
    uint8_t connection_info;
    struct datum version;
    struct datum dcid;
    struct datum scid;
    struct datum token;
    struct datum data;
    bool valid;

    quic_initial_packet(struct datum &d) : connection_info{0}, dcid{NULL, NULL}, scid{NULL, NULL}, token{NULL, NULL}, data{NULL, NULL}, valid{false} {
        parse(d);
    }

    void parse(struct datum &d) {
        d.read_uint8(&connection_info);
        if ((connection_info & 0x30) != 0) {
            return;
        }

        version.parse(d, 4);

        uint8_t dcid_length;
        d.read_uint8(&dcid_length);
        dcid.parse(d, dcid_length);

        uint8_t scid_length;
        d.read_uint8(&scid_length);
        scid.parse(d, scid_length);

        //variable_length_integer var_int;
        ssize_t var_int = parse_variable_length_integer(d);
        token.parse(d, var_int);

        // @TODO: need to handle actually handle QUIC's variable length encoding
        uint16_t data_length;
        d.read_uint16(&data_length);
        data_length = data_length & 0x3FFF;
        data.parse(d, data_length);

        if ((data.is_not_empty() == false) || (data_length < 32) ||
            (dcid.is_not_empty() == false)) {
            return;  // invalid or incomplete packet
        }
        valid = true;
    }

    bool is_not_empty() {
        return valid;
    }

    void write_json(struct json_object &o, bool =false) const {
        if (!valid) {
            return;
        }

        struct json_object json_quic{o, "quic"};
        struct uint8_bitfield bitfield{connection_info};
        json_quic.print_key_value("connection_info", bitfield);
        json_quic.print_key_hex("version", version);
        json_quic.print_key_hex("dcid", dcid);
        json_quic.print_key_hex("scid", scid);
        json_quic.print_key_hex("token", token);
        json_quic.print_key_hex("data", data);
        json_quic.close();

    }

};

class quic_parameters {
    std::unordered_map<uint32_t, uint8_t*> quic_initial_salt;

    uint8_t salt_d22[20]     = {0x7f,0xbc,0xdb,0x0e,0x7c,0x66,0xbb,0xe9,0x19,0x3a,0x96,0xcd,0x21,0x51,0x9e,0xbd,0x7a,0x02,0x64,0x4a};
    uint8_t salt_d23_d28[20] = {0xc3,0xee,0xf7,0x12,0xc7,0x2e,0xbb,0x5a,0x11,0xa7,0xd2,0x43,0x2b,0xb4,0x63,0x65,0xbe,0xf9,0xf5,0x02};
    uint8_t salt_d29_d32[20] = {0xaf,0xbf,0xec,0x28,0x99,0x93,0xd2,0x4c,0x9e,0x97,0x86,0xf1,0x9c,0x61,0x11,0xe0,0x43,0x90,0xa8,0x99};
    uint8_t salt_d33_v1[20]  = {0x38,0x76,0x2c,0xf7,0xf5,0x59,0x34,0xb3,0x4d,0x17,0x9a,0xe6,0xa4,0xc8,0x0c,0xad,0xcc,0xbb,0x7f,0x0a};


public:

    quic_parameters() {
        quic_initial_salt = {
            {4278190102, salt_d22},     // draft-22
            {4278190103, salt_d23_d28}, // draft-23
            {4278190104, salt_d23_d28}, // draft-24
            {4278190105, salt_d23_d28}, // draft-25
            {4278190106, salt_d23_d28}, // draft-26
            {4278190107, salt_d23_d28}, // draft-27
            {4278190108, salt_d23_d28}, // draft-28
            {4278190109, salt_d29_d32}, // draft-29
            {4278190110, salt_d29_d32}, // draft-30
            {4278190111, salt_d29_d32}, // draft-31
            {4278190112, salt_d29_d32}, // draft-32
            {4278190113, salt_d33_v1},  // draft-33
            {4278190114, salt_d33_v1},  // draft-34
            {1,          salt_d33_v1},  // version-1
        };
    }

    uint8_t *get_initial_salt(uint32_t version) {
        auto pair = quic_initial_salt.find(version);
        if (pair != quic_initial_salt.end()) {
            return pair->second;
        } else {
            return nullptr;
        }
    }

    static quic_parameters &create() {
        static quic_parameters *quic_params = new quic_parameters;
        return *quic_params;
    }
};


struct quic_initial_packet_crypto {
    bool valid;

    constexpr static const uint8_t client_in_label[] = "tls13 client in";
    constexpr static const uint8_t quic_key_label[]  = "tls13 quic key";
    constexpr static const uint8_t quic_iv_label[]   = "tls13 quic iv";
    constexpr static const uint8_t quic_hp_label[]   = "tls13 quic hp";

    size_t salt_length = 20;

    uint8_t quic_key[EVP_MAX_MD_SIZE] = {0};
    unsigned int quic_key_len = 0;

    uint8_t quic_iv[EVP_MAX_MD_SIZE] = {0};
    unsigned int quic_iv_len = 0;

    uint8_t quic_hp[EVP_MAX_MD_SIZE] = {0};
    unsigned int quic_hp_len = 0;

    uint8_t pn_length = 0;

    unsigned char plaintext[1024] = {0};
    int16_t plaintext_len = 0;

    quic_initial_packet_crypto(struct quic_initial_packet quic_pkt) : valid{false} {
        if (!quic_pkt.is_not_empty()) {
            return;
        }
        const uint8_t *dcid = quic_pkt.dcid.data;
        size_t dcid_len = quic_pkt.dcid.length();
        uint32_t version = ntohl(*((uint32_t*)quic_pkt.version.data));

        static quic_parameters &quic_params = quic_parameters::create();  // initialize on first use
        uint8_t *initial_salt = quic_params.get_initial_salt(version);
        if (initial_salt == nullptr) {
            return;
        }

        uint8_t initial_secret[EVP_MAX_MD_SIZE];
        unsigned int initial_secret_len = 0;
        HMAC(EVP_sha256(), initial_salt, salt_length, dcid, dcid_len, initial_secret, &initial_secret_len);

        uint8_t c_initial_secret[EVP_MAX_MD_SIZE] = {0};
        unsigned int c_initial_secret_len = 0;
        kdf_tls13(initial_secret, initial_secret_len, client_in_label, sizeof(client_in_label)-1, 32, c_initial_secret, &c_initial_secret_len);
        kdf_tls13(c_initial_secret, c_initial_secret_len, quic_key_label, sizeof(quic_key_label)-1, 16, quic_key, &quic_key_len);
        kdf_tls13(c_initial_secret, c_initial_secret_len, quic_iv_label, sizeof(quic_iv_label)-1, 12, quic_iv, &quic_iv_len);
        kdf_tls13(c_initial_secret, c_initial_secret_len, quic_hp_label, sizeof(quic_hp_label)-1, 16, quic_hp, &quic_hp_len);

        AES_KEY enc_key;
        AES_set_encrypt_key(quic_hp, 128, &enc_key);
        uint8_t buf[32] = {0};
        AES_encrypt(quic_pkt.data.data+4, buf, &enc_key);
        pn_length = quic_pkt.connection_info ^ (buf[0] & 0x0f);
        pn_length = (pn_length & 0x03) + 1;


        for (uint8_t i = quic_iv_len-pn_length; i < quic_iv_len; i++) {
            quic_iv[i] ^= (buf[(i-(quic_iv_len-pn_length))+1] ^ *(quic_pkt.data.data + (i-(quic_iv_len-pn_length))));
        }

        valid = true;
    }

    void decrypt(const uint8_t *data, unsigned int length) {
        uint16_t cipher_len = (length-pn_length < 1024) ? length-pn_length : 1024;
        plaintext_len = gcm_decrypt(data+pn_length, cipher_len, quic_key, quic_iv, plaintext);
        if (plaintext_len == -1) { // error
            valid = false;
            return;
        }

        if ((plaintext[4] != 0x01) || (plaintext[8] != 0x03) || (plaintext[9] != 0x03)) {
            valid = false;
            return;
        }

    }

    // adapted from https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption
    int gcm_decrypt(const unsigned char *ciphertext, int ciphertext_len,
                    unsigned char *key, unsigned char *iv,
                    unsigned char *plaintext)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new()))
            return -1;

        /* Initialise the decryption operation. */
        if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
            return -1;

        /* Initialise key and IV */
        if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
            return -1;

        /*
         * Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary
         */
        if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
            return -1;
        plaintext_len = len;

        /*
         * Finalise the decryption. A positive return value indicates success,
         * anything else is a failure - the plaintext is not trustworthy.
         */
        EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        plaintext_len += len;
        return plaintext_len;
    }

    void kdf_tls13(uint8_t *secret, unsigned int secret_length, const uint8_t *label, const unsigned int label_len,
                   uint8_t length, uint8_t *out_, unsigned int *out_len) {
        uint8_t *new_label = new uint8_t[4+label_len]();
        new_label[1] = length;
        new_label[2] = label_len;
        for (uint i = 0; i < label_len; i++) {
            new_label[3+i] = label[i];
        }

        uint8_t ind = 0;
        uint8_t block[EVP_MAX_MD_SIZE];
        uint8_t new_block[512] = {0};
        unsigned int block_len = 0;
        while (*out_len < length) {
            ind++;

            for (unsigned int i = 0; i < block_len; i++) {
                new_block[i] = block[i];
            }
            for (unsigned int i = block_len; i < block_len+label_len+4; i++) {
                new_block[i] = new_label[i];
            }
            new_block[block_len+label_len+4] = ind;

            HMAC(EVP_sha256(), secret, secret_length, new_block, block_len+label_len+5, block, &block_len);

            for (unsigned int i = 0; i < block_len; i++) {
                out_[*out_len] = block[i];
                (*out_len)++;
                if (*out_len >= length) {
                    delete[] new_label;
                    return ;
                }
            }
        }
        delete[] new_label;
    }

    bool is_not_empty() const {
        return valid;
    }

    datum get_plaintext() const {
        struct datum quic_plaintext{plaintext+8, plaintext+plaintext_len};
        return quic_plaintext;
    }
};

constexpr uint8_t quic_initial_packet_crypto::client_in_label[];
constexpr uint8_t quic_initial_packet_crypto::quic_key_label[];
constexpr uint8_t quic_initial_packet_crypto::quic_iv_label[];
constexpr uint8_t quic_initial_packet_crypto::quic_hp_label[];


//   Version Negotiation Packet {
//     Header Form (1) = 1,
//     Unused (7),
//     Version (32) = 0,
//     Destination Connection ID Length (8),
//     Destination Connection ID (0..2040),
//     Source Connection ID Length (8),
//     Source Connection ID (0..2040),
//     Supported Version (32) ...,
//   }
//
struct quic_version_negotiation {
    uint8_t connection_info;
    struct datum dcid;
    struct datum scid;
    struct datum version_list;
    bool valid;

    quic_version_negotiation(struct datum &d) : connection_info{0}, dcid{NULL, NULL}, scid{NULL, NULL}, version_list{NULL, NULL}, valid{false} {
        parse(d);
    }

    void parse(struct datum &d) {
        d.read_uint8(&connection_info);
        if ((connection_info & 0x80) != 0x80) {
            return;
        }
        d.skip(4);  // skip version, it's 00000000

        uint8_t dcid_length;
        d.read_uint8(&dcid_length);
        dcid.parse(d, dcid_length);

        uint8_t scid_length;
        d.read_uint8(&scid_length);
        scid.parse(d, scid_length);

        version_list = d;  // TODO: member function to get remainder

        if ((version_list.is_not_empty() == false) || (dcid.is_not_empty() == false)) {
            return;  // invalid or incomplete packet
        }
        valid = true;
    }

    bool is_not_empty() {
        return valid;
    }

    void write_json(struct json_object &o) const {
        if (!valid) {
            return;
        }

        struct uint8_bitfield bitfield{connection_info};
        o.print_key_value("connection_info", bitfield);
        o.print_key_hex("dcid", dcid);
        o.print_key_hex("scid", scid);
        json_array array{o, "versions"};
        datum tmp = version_list;
        while (tmp.is_not_empty()) {
            datum version;
            version.parse(version, 4);
            array.print_hex(version);
        }
        array.close();
    }

    // TODO: add mask and value
    //
    // mask:  80ffffffff...
    // value: 8000000000...
};


// class quic_init represents an initial quic message
//
class quic_init {
    quic_initial_packet initial_packet;
    quic_initial_packet_crypto quic_pkt_crypto;
    tls_client_hello hello;

public:

    quic_init(struct datum &d) : initial_packet{d}, quic_pkt_crypto{initial_packet}, hello{} {
        if (quic_pkt_crypto.is_not_empty()) {
            quic_pkt_crypto.decrypt(initial_packet.data.data, initial_packet.data.length());
            datum plaintext = quic_pkt_crypto.get_plaintext();
            hello.parse(plaintext);
        }
    }

    bool is_not_empty() {
        return initial_packet.is_not_empty();
    }

    void write_json(struct json_object &record, bool metadata_output=false) {
        if (quic_pkt_crypto.is_not_empty()) {
            if (hello.is_not_empty()) {
                hello.write_json(record, metadata_output);
            }
        }
        initial_packet.write_json(record);
    }

    void compute_fingerprint(struct fingerprint &fp) const {
        if (quic_pkt_crypto.is_not_empty()) {
            fp.set(hello, fingerprint_type_quic);
        }
    }

    bool do_analysis(const struct key &k_, struct analysis_context &analysis_, classifier *c_) {
        struct datum sn{NULL, NULL};
        hello.extensions.set_server_name(sn);

        analysis_.destination.init(sn, k_);

        return c_->analyze_fingerprint_and_destination_context(analysis_.fp, analysis_.destination, analysis_.result);
    }
};


#endif /* QUIC_H */
