/* file-rtpdump.c
 *
 * Routines for rtpdump file dissection
 * Copyright 2023, David Perry <boolean263@protonmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This dissects the rtpdump file format as generated by Wireshark.
 * See also https://wiki.wireshark.org/rtpdump
 * This file format was created as part of rtptools:
 * https://github.com/irtlab/rtptools
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/expert.h>
#include <wsutil/strtoi.h>
#include <wsutil/inet_addr.h>

void proto_register_rtpdump(void);
void proto_reg_handoff_rtpdump(void);

/* Initialize the protocol and registered fields */
static int proto_rtpdump;

static gint hf_rtpdump_text_header;
static gint hf_rtpdump_play_program;
static gint hf_rtpdump_version;
static gint hf_rtpdump_txt_ipv4;
static gint hf_rtpdump_txt_ipv6;
static gint hf_rtpdump_txt_port;

static gint hf_rtpdump_binary_header;
static gint hf_rtpdump_ts_sec;
static gint hf_rtpdump_ts_usec;
static gint hf_rtpdump_ts;
static gint hf_rtpdump_bin_addr;
static gint hf_rtpdump_bin_port;
static gint hf_rtpdump_padding;

static gint hf_rtpdump_pkt;
static gint hf_rtpdump_pkt_len;
static gint hf_rtpdump_pkt_plen;
static gint hf_rtpdump_pkt_offset;
static gint hf_rtpdump_pkt_data;

/* Initialize the subtree pointers */
static gint ett_rtpdump;
static gint ett_rtpdump_text_header;
static gint ett_rtpdump_binary_header;
static gint ett_rtpdump_pkt;

static expert_field ei_rtpdump_unknown_program;
static expert_field ei_rtpdump_unknown_version;
static expert_field ei_rtpdump_bad_txt_addr;
static expert_field ei_rtpdump_bad_txt_port;
static expert_field ei_rtpdump_bin_ipv6;
static expert_field ei_rtpdump_addrs_match;
static expert_field ei_rtpdump_addrs_mismatch;
static expert_field ei_rtpdump_caplen;

/* Reasonable minimum length for the RTP header (including the magic):
 * - 13 for "#!rtpplay1.0 "
 * - WS_INET_ADDRSTRLEN characters for a destination IPv4 address
 * - 1 for a slash
 * - 3 characters for a destination port number
 * - 1 character for a newline
 * - 4 bytes for each of start seconds, start useconds, source IPv4
 * - 2 bytes for each of source port, padding
 */
#define RTP_HEADER_MIN_LEN 24+WS_INET_ADDRSTRLEN

static gint
dissect_rtpdump(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, void *data _U_)
{
    proto_tree *tree, *subtree;
    proto_item *ti;
    gint tvb_len = tvb_captured_length(tvb);
    guint pkt_num = 1;
    static const guint8 shebang[] = {'#', '!'};
    static const char rtpplay[] = "rtpplay";
    static const char rtpver[] = "1.0";
    gint offset = 0;
    gint i = 0;
    gint slash = 0;
    gint eol = 0;
    gint space = 0;
    guint8 *str = NULL;
    guint16 txt_port = 0;
    guint32 bin_port = 0;
    ws_in4_addr txt_ipv4 = 0;
    ws_in6_addr txt_ipv6 = {0};
    ws_in4_addr bin_ipv4 = 0;
    gboolean txt_is_ipv6 = FALSE;
    nstime_t start_time = NSTIME_INIT_ZERO;
    gint pkt_length;
    gint data_length;

    if (tvb_len < RTP_HEADER_MIN_LEN)
        return 0;
    if (0 != tvb_memeql(tvb, 0, shebang, sizeof(shebang)))
        return 0;
    if (-1 == (eol = tvb_find_guint8(tvb, 0, -1, '\n')) ||
            -1 == (slash = tvb_find_guint8(tvb, 0, eol, '/')) ||
            -1 == (space = tvb_find_guint8(tvb, 0, slash, ' '))) {
        return 0;
    }

    ti = proto_tree_add_item(parent_tree, proto_rtpdump, tvb, offset, -1, ENC_NA);
    tree = proto_item_add_subtree(ti, ett_rtpdump);

    /* Handle the text header */
    ti = proto_tree_add_item(tree, hf_rtpdump_text_header, tvb, offset, eol+1, ENC_ASCII);
    subtree = proto_item_add_subtree(ti, ett_rtpdump_text_header);

    /* Get the program name */
    offset += 2;
    for (i = offset; g_ascii_isalpha(tvb_get_guint8(tvb, i)); i++)
        /* empty loop */ ;
    ti = proto_tree_add_item_ret_string(subtree, hf_rtpdump_play_program,
                                        tvb, offset, i-offset, ENC_ASCII,
                                        pinfo->pool, (const guint8 **)&str);
    if (0 != g_strcmp0(str, rtpplay)) {
        expert_add_info(pinfo, ti, &ei_rtpdump_unknown_program);
    }

    /* Get the program version */
    offset = i;
    ti = proto_tree_add_item_ret_string(subtree, hf_rtpdump_version,
                                        tvb, offset, space-offset, ENC_ASCII,
                                        pinfo->pool, (const guint8 **)&str);
    if (0 != g_strcmp0(str, rtpver)) {
        expert_add_info(pinfo, ti, &ei_rtpdump_unknown_version);
    }

    /* Get the text IP */
    offset = space + 1;
    str = tvb_get_string_enc(pinfo->pool, tvb, offset, slash-offset, ENC_ASCII);
    if (ws_inet_pton4(str, &txt_ipv4)) {
        proto_tree_add_ipv4(subtree, hf_rtpdump_txt_ipv4, tvb, offset, slash-offset, txt_ipv4);
    }
    else if (ws_inet_pton6(str, &txt_ipv6)) {
        txt_is_ipv6 = TRUE;
        proto_tree_add_ipv6(subtree, hf_rtpdump_txt_ipv6, tvb, offset, slash-offset, &txt_ipv6);
    }
    else {
        proto_tree_add_expert(subtree, pinfo, &ei_rtpdump_bad_txt_addr,
                              tvb, offset, eol-offset);
    }

    /* Get the text port */
    offset = slash + 1;
    str = tvb_get_string_enc(pinfo->pool, tvb, offset, eol-offset, ENC_ASCII);
    if (ws_strtou16(str, NULL, &txt_port)) {
        proto_tree_add_uint(subtree, hf_rtpdump_txt_port, tvb, offset, eol-offset, txt_port);
    }
    else {
        proto_tree_add_expert(subtree, pinfo, &ei_rtpdump_bad_txt_port,
                              tvb, offset, eol-offset);
    }

    /* Handle the binary header */
    offset = eol + 1;
    ti = proto_tree_add_item(tree, hf_rtpdump_binary_header, tvb, offset, 16, ENC_NA);
    subtree = proto_item_add_subtree(ti, ett_rtpdump_binary_header);

    proto_tree_add_item_ret_uint(subtree, hf_rtpdump_ts_sec, tvb, offset, 4, ENC_BIG_ENDIAN,
                                 (guint32 *)&start_time.secs);
    proto_tree_add_item_ret_uint(subtree, hf_rtpdump_ts_usec, tvb, offset+4, 4, ENC_BIG_ENDIAN,
                                 &start_time.nsecs);
    start_time.nsecs *= 1000;
    ti = proto_tree_add_time(subtree, hf_rtpdump_ts, tvb, offset, 8, &start_time);
    proto_item_set_generated(ti);
    offset += 8;

    ti = proto_tree_add_item(subtree, hf_rtpdump_bin_addr, tvb, offset, 4, ENC_BIG_ENDIAN);
    /* Force internal representation to big-endian as per wsutil/inet_ipv4.h */
    bin_ipv4 = g_htonl(tvb_get_guint32(tvb, offset, ENC_BIG_ENDIAN));
    offset += 4;
    proto_tree_add_item_ret_uint(subtree, hf_rtpdump_bin_port, tvb, offset, 2, ENC_BIG_ENDIAN, &bin_port);
    offset += 2;
    proto_tree_add_item(subtree, hf_rtpdump_padding, tvb, offset, 2, ENC_NA);
    offset += 2;

    if (txt_is_ipv6) {
        expert_add_info(pinfo, ti, &ei_rtpdump_bin_ipv6);
        expert_add_info(pinfo, subtree, &ei_rtpdump_addrs_mismatch);
    }
    else if(bin_ipv4 == txt_ipv4 && bin_port == txt_port) {
        expert_add_info(pinfo, subtree, &ei_rtpdump_addrs_match);
    }
    else {
        expert_add_info(pinfo, subtree, &ei_rtpdump_addrs_mismatch);
    }

    /* Handle individual packets */
    while (offset < tvb_len) {
        pkt_length = tvb_get_ntohs(tvb, offset);
        ti = proto_tree_add_item(tree, hf_rtpdump_pkt, tvb, offset, pkt_length, ENC_NA);
        subtree = proto_item_add_subtree(ti, ett_rtpdump_pkt);
        proto_item_set_text(subtree, "Packet %d", pkt_num++);

        pkt_length -= 8;

        proto_tree_add_item(subtree, hf_rtpdump_pkt_len, tvb, offset, 2, ENC_BIG_ENDIAN);
        offset += 2;

        ti = proto_tree_add_item_ret_uint(subtree, hf_rtpdump_pkt_plen, tvb, offset, 2, ENC_BIG_ENDIAN,
                                     &data_length);
        if (data_length > pkt_length) {
            expert_add_info(pinfo, ti, &ei_rtpdump_caplen);
        }
        offset += 2;

        proto_tree_add_item(subtree, hf_rtpdump_pkt_offset, tvb, offset, 4, ENC_BIG_ENDIAN);
        offset += 4;

        proto_tree_add_item(subtree, hf_rtpdump_pkt_data, tvb, offset, pkt_length, ENC_NA);
        offset += pkt_length;
    }

    return tvb_len;
}

static bool
dissect_rtpdump_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, void *data)
{
    return dissect_rtpdump(tvb, pinfo, parent_tree, data) > 0;
}

/****************** Register the protocol with Wireshark ******************/

void
proto_register_rtpdump(void)
{
    static hf_register_info hf[] = {
        { &hf_rtpdump_text_header,
            { "Text header", "rtpdump.text_header",
              FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_play_program,
            { "Play program", "rtpdump.play_program",
              FT_STRING, BASE_NONE, NULL, 0x0,
              "Program to be used to play this stream", HFILL }
        },
        { &hf_rtpdump_version,
            { "File format version", "rtpdump.version",
              FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_txt_ipv4,
            { "Text IPv4 address", "rtpdump.txt_addr",
              FT_IPv4, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_txt_ipv6,
            { "Text IPv6 address", "rtpdump.txt_addr",
              FT_IPv6, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_txt_port,
            { "Text port", "rtpdump.txt_port",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_binary_header,
            { "Binary header", "rtpdump.binary_header",
              FT_BYTES, BASE_NONE|BASE_NO_DISPLAY_VALUE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_ts_sec,
            { "Start time (seconds)", "rtpdump.ts.sec",
              FT_UINT32, BASE_DEC, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_ts_usec,
            { "Start time (microseconds)", "rtpdump.ts_usec",
              FT_UINT32, BASE_DEC, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_ts,
            { "Start time", "rtpdump.ts",
              FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_bin_addr,
            { "Binary IPv4 address", "rtpdump.bin_addr",
              FT_IPv4, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_bin_port,
            { "Binary port", "rtpdump.bin_port",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_padding,
            { "Padding", "rtpdump.padding",
              FT_BYTES, BASE_NONE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_pkt,
            { "Packet", "rtpdump.packet",
              FT_BYTES, BASE_NONE|BASE_NO_DISPLAY_VALUE, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_pkt_len,
            { "Packet length", "rtpdump.pkt_len",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "Total packet length", HFILL }
        },
        { &hf_rtpdump_pkt_plen,
            { "Data length", "rtpdump.pkt_plen",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              NULL, HFILL }
        },
        { &hf_rtpdump_pkt_offset,
            { "Time offset (milliseconds)", "rtpdump.pkt_offset",
              FT_UINT32, BASE_DEC, NULL, 0x0,
              "Time from start of capture", HFILL }
        },
        { &hf_rtpdump_pkt_data,
            { "Data", "rtpdump.pkt_data",
              FT_BYTES, BASE_NONE|BASE_NO_DISPLAY_VALUE, NULL, 0x0,
              NULL, HFILL }
        },
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
        &ett_rtpdump,
        &ett_rtpdump_text_header,
        &ett_rtpdump_binary_header,
        &ett_rtpdump_pkt,
    };

    static ei_register_info ei[] = {
        { &ei_rtpdump_unknown_program,
          { "rtpdump.play_program.unknown", PI_PROTOCOL, PI_WARN,
            "Playback program not the expected 'rtpplay', dissection may be incorrect", EXPFILL }},
        { &ei_rtpdump_unknown_version,
          { "rtpdump.version.unknown", PI_PROTOCOL, PI_WARN,
            "Version not recognized, dissection may be incorrect", EXPFILL }},
        { &ei_rtpdump_bad_txt_addr,
          { "rtpdump.txt_addr.bad", PI_PROTOCOL, PI_WARN,
            "Unparseable text address", EXPFILL }},
        { &ei_rtpdump_bad_txt_port,
          { "rtpdump.txt_port.bad", PI_PROTOCOL, PI_WARN,
            "Unparseable text port", EXPFILL }},
        { &ei_rtpdump_bin_ipv6,
          { "rtpdump.bin_addr.ipv6", PI_PROTOCOL, PI_NOTE,
            "Binary IPv4 address may be a truncated IPv6 address", EXPFILL }},
        { &ei_rtpdump_addrs_match,
          { "rtpdump.address.match", PI_PROTOCOL, PI_CHAT,
            "Text and binary addresses and ports match -- file likely generated by rtpdump", EXPFILL }},
        { &ei_rtpdump_addrs_mismatch,
          { "rtpdump.address.mismatch", PI_PROTOCOL, PI_CHAT,
            "Text and binary addresses and ports do not match -- file likely generated by wireshark", EXPFILL }},
        { &ei_rtpdump_caplen,
          { "rtpdump.pkt_plen.truncated", PI_PROTOCOL, PI_NOTE,
            "Data was truncated during capture", EXPFILL }},
    };

    expert_module_t* expert_rtpdump;

    /* Register the protocol name and description */
    proto_rtpdump = proto_register_protocol("RTPDump file format", "rtpdump", "rtpdump");

    /* Required function calls to register the header fields
     * and subtrees used */
    proto_register_field_array(proto_rtpdump, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

    expert_rtpdump = expert_register_protocol(proto_rtpdump);
    expert_register_field_array(expert_rtpdump, ei, array_length(ei));

    register_dissector("rtpdump", dissect_rtpdump, proto_rtpdump);
}

void
proto_reg_handoff_rtpdump(void)
{
    heur_dissector_add("wtap_file", dissect_rtpdump_heur, "RTPDump file", "rtpdump_wtap", proto_rtpdump, HEURISTIC_ENABLE);
}
