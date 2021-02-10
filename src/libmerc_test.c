/*
 * libmerc_test.c
 *
 * main() file for libmerc.so test driver program
 *
 * Copyright (c) 2020 Cisco Systems, Inc. All rights reserved.  License at
 * https://github.com/cisco/mercury/blob/master/LICENSE
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <strings.h>
#include <sys/time.h>

#include "libmerc/libmerc.h"
#include "libmerc/result.h"

unsigned char client_hello_eth[] = {
  0x00, 0x50, 0x56, 0xe0, 0xb0, 0xbc, 0x00, 0x0c, 0x29, 0x74, 0x82, 0x2f,
  0x08, 0x00, 0x45, 0x00, 0x01, 0x61, 0xd5, 0xeb, 0x40, 0x00, 0x40, 0x06,
  0x58, 0x0c, 0xc0, 0xa8, 0x71, 0xed, 0x97, 0x65, 0x41, 0xa4, 0x80, 0x2a,
  0x01, 0xbb, 0xdd, 0x07, 0xfe, 0x40, 0x25, 0x00, 0x2e, 0x63, 0x50, 0x18,
  0xfa, 0xf0, 0x0c, 0xf3, 0x00, 0x00, 0x16, 0x03, 0x01, 0x01, 0x34, 0x01,
  0x00, 0x01, 0x30, 0x03, 0x03, 0x5b, 0x1f, 0x43, 0x3b, 0x2f, 0x09, 0x1c,
  0x61, 0xff, 0xd5, 0x1d, 0x3d, 0x8f, 0x00, 0x8f, 0xea, 0x86, 0x3f, 0xb6,
  0xc3, 0x72, 0x6e, 0x7f, 0x05, 0x6b, 0x01, 0x9e, 0xc7, 0x68, 0xcd, 0x12,
  0x58, 0x20, 0xf0, 0xa3, 0x04, 0x3a, 0x4f, 0x60, 0x89, 0x7b, 0x16, 0x89,
  0xf7, 0x46, 0xcf, 0x3c, 0x69, 0x03, 0xf9, 0xf6, 0x06, 0xa7, 0x7f, 0x53,
  0x36, 0xd4, 0xe2, 0x16, 0x33, 0xe9, 0x88, 0x48, 0xff, 0x14, 0x00, 0x3e,
  0x13, 0x02, 0x13, 0x03, 0x13, 0x01, 0xc0, 0x2c, 0xc0, 0x30, 0x00, 0x9f,
  0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xc0, 0x2b, 0xc0, 0x2f, 0x00, 0x9e,
  0xc0, 0x24, 0xc0, 0x28, 0x00, 0x6b, 0xc0, 0x23, 0xc0, 0x27, 0x00, 0x67,
  0xc0, 0x0a, 0xc0, 0x14, 0x00, 0x39, 0xc0, 0x09, 0xc0, 0x13, 0x00, 0x33,
  0x00, 0x9d, 0x00, 0x9c, 0x00, 0x3d, 0x00, 0x3c, 0x00, 0x35, 0x00, 0x2f,
  0x00, 0xff, 0x01, 0x00, 0x00, 0xa9, 0x00, 0x00, 0x00, 0x10, 0x00, 0x0e,
  0x00, 0x00, 0x0b, 0x6e, 0x79, 0x74, 0x69, 0x6d, 0x65, 0x73, 0x2e, 0x63,
  0x6f, 0x6d, 0x00, 0x0b, 0x00, 0x04, 0x03, 0x00, 0x01, 0x02, 0x00, 0x0a,
  0x00, 0x0c, 0x00, 0x0a, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x1e, 0x00, 0x19,
  0x00, 0x18, 0x00, 0x23, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17,
  0x00, 0x00, 0x00, 0x0d, 0x00, 0x30, 0x00, 0x2e, 0x04, 0x03, 0x05, 0x03,
  0x06, 0x03, 0x08, 0x07, 0x08, 0x08, 0x08, 0x09, 0x08, 0x0a, 0x08, 0x0b,
  0x08, 0x04, 0x08, 0x05, 0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01,
  0x03, 0x03, 0x02, 0x03, 0x03, 0x01, 0x02, 0x01, 0x03, 0x02, 0x02, 0x02,
  0x04, 0x02, 0x05, 0x02, 0x06, 0x02, 0x00, 0x2b, 0x00, 0x09, 0x08, 0x03,
  0x04, 0x03, 0x03, 0x03, 0x02, 0x03, 0x01, 0x00, 0x2d, 0x00, 0x02, 0x01,
  0x01, 0x00, 0x33, 0x00, 0x26, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x20, 0x7f,
  0x36, 0x7d, 0x60, 0x25, 0x06, 0x55, 0xca, 0xbb, 0x18, 0xd3, 0x4c, 0x84,
  0xcc, 0x5b, 0x14, 0xcd, 0x0a, 0x95, 0xe9, 0x06, 0x13, 0x5d, 0xd7, 0x6a,
  0xee, 0x62, 0x2b, 0x2b, 0x54, 0x1c, 0x17
};

uint8_t *client_hello_ip = client_hello_eth + 14;
size_t client_hello_ip_len = sizeof(client_hello_eth) - 14;
//unsigned int client_hello_eth_len = 367;

char *expected_json = "{\"fingerprints\":{\"tls\":\"(0303)(130213031301c02cc030009fcca9cca8ccaac02bc02f009ec024c028006bc023c0270067c00ac0140039c009c0130033009d009c003d003c0035002f00ff)((0000)(000b000403000102)(000a000c000a001d0017001e00190018)(0023)(0016)(0017)(000d0030002e040305030603080708080809080a080b080408050806040105010601030302030301020103020202040205020602)(002b0009080304030303020301)(002d00020101)(0033))\"},\"tls\":{\"client\":{\"server_name\":\"nytimes.com\"}},\"analysis\":{\"process\":\"siege\",\"score\":0.882271,\"os_info\":{\"(Mac OS X)(High Sierra)(10.13.6)\":1252897}},\"src_ip\":\"192.168.113.237\",\"dst_ip\":\"151.101.65.164\",\"protocol\":6,\"src_port\":32810,\"dst_port\":443,\"event_start\":0.000000}\n";

int main(int argc, char *argv[]) {

    // initialize library's global configuration
    struct libmerc_config config = libmerc_config_init();
    config.do_analysis = true;
    config.resources = "../resources";
    config.report_os = true;
    int verbosity = 0;

    int retval = mercury_init(&config, verbosity);
    if (retval) {
        fprintf(stderr, "mercury_init() error (code %d)\n", retval);
        return EXIT_FAILURE;
    }

    // initialize per-thread state
    mercury_packet_processor m = mercury_packet_processor_construct();
    if (m == NULL) {
        fprintf(stderr, "error in mercury_packet_processor_construct()\n");
        return EXIT_FAILURE;
    }

    char output_buffer[4096] = { 0, };
    struct timespec time;
    time.tv_sec = time.tv_nsec = 0;  // set to January 1st, 1970 (the Epoch)
    size_t num_bytes_written = mercury_packet_processor_write_json(m, output_buffer, sizeof(output_buffer), client_hello_eth, sizeof(client_hello_eth), &time);
    if (num_bytes_written == 0) {
        fprintf(stdout, "error in mercury_packet_processor_write_json() (wrote %zu bytes)\n", num_bytes_written);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "%s", output_buffer);
    if (memcmp(expected_json, output_buffer, num_bytes_written) != 0) {
        fprintf(stdout, "error in output of mercury_packet_processor_write_json() (got %s)\n", output_buffer);
    }

    bzero(output_buffer, sizeof(output_buffer));
    num_bytes_written = mercury_packet_processor_ip_write_json(m, output_buffer, sizeof(output_buffer), client_hello_ip, client_hello_ip_len, &time);
    if (num_bytes_written == 0) {
        fprintf(stdout, "error in mercury_packet_processor_ip_write_json() (wrote %zu bytes)\n", num_bytes_written);
        return EXIT_FAILURE;
    }
    fprintf(stdout, "%s", output_buffer);
    if (memcmp(expected_json, output_buffer, num_bytes_written) != 0) {
        fprintf(stdout, "error in output of mercury_packet_processor_ip_write_json() (got %s)\n", output_buffer);
    }

    // test analysis_result interface
    const struct analysis_context *c = mercury_packet_processor_ip_get_analysis_context(m, client_hello_ip, client_hello_ip_len, &time);
    if (c && c->fp.type == fingerprint_type_tls) {
        fprintf(stdout, "got analysis result\n");
        fprintf(stdout, "fingerprint: %s\n", c->fp.fp_str);
        fprintf(stdout, "server_name: \"%s\"\n", c->destination.sn_str);
        fprintf(stdout, "process name: \"%s\"\n", c->result.max_proc);
        fprintf(stdout, "probability score: %Lf\n", c->result.max_score);
        for (unsigned int i=0; i < c->result.os_info_len; i++) {
            struct os_information *os = &c->result.os_info[i];
            fprintf(stdout, "OS and prevalence: %s\t%lu\n", os->os_name, os->os_prevalence);
        }
    }

    // tear down per-thread state
    mercury_packet_processor_destruct(m);

    // tear down library's global configuration
    retval = mercury_finalize();
    if (retval) {
        fprintf(stderr, "mercury_finalize() error (code %d)\n", retval);
        return EXIT_FAILURE;
    }

    return 0;
}
