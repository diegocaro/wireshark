/* wimax_cdma_code_decoder.c
 * WiMax CDMA CODE Attribute decoder
 *
 * Copyright (c) 2007 by Intel Corporation.
 *
 * Author: Lu Pan <lu.pan@intel.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Include files */

#include "config.h"

#include <epan/packet.h>
#include "wimax-int.h"

static int proto_wimax_cdma_code_decoder;
static int ett_wimax_cdma_code_decoder;

static int hf_wimax_ranging_code;
static int hf_wimax_ranging_symbol_offset;
static int hf_wimax_ranging_subchannel_offset;

static int dissect_wimax_cdma_code_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	int offset = 0;
	proto_item *cdma_item;
	proto_tree *cdma_tree;

	/* update the info column */
	col_append_sep_str(pinfo->cinfo, COL_INFO, NULL, "CDMA Code Attribute");
	if (tree)
	{	/* we are being asked for details */
		/* display CDMA dissector info */
		cdma_item = proto_tree_add_item(tree, proto_wimax_cdma_code_decoder, tvb, offset, -1, ENC_NA);
		/* add CDMA Code subtree */
		cdma_tree = proto_item_add_subtree(cdma_item, ett_wimax_cdma_code_decoder);
		/* display the first CDMA Code */
		proto_tree_add_item(cdma_tree, hf_wimax_ranging_code, tvb, offset, 1, ENC_BIG_ENDIAN);
		/* display the 2nd CDMA Code */
		proto_tree_add_item(cdma_tree, hf_wimax_ranging_symbol_offset, tvb, offset+1, 1, ENC_BIG_ENDIAN);
		/* display the 3rd CDMA Code */
		proto_tree_add_item(cdma_tree, hf_wimax_ranging_subchannel_offset, tvb, offset+2, 1, ENC_BIG_ENDIAN);
	}
	return tvb_captured_length(tvb);
}

/* Register Wimax CDMA Protocol */
void wimax_proto_register_wimax_cdma(void)
{
	/* TLV display */
	static hf_register_info hf[] =
	{
		{
			&hf_wimax_ranging_code,
			{
				"Ranging Code", "wmx.cdma.ranging_code",
				FT_UINT8, BASE_HEX, NULL, 0x0,
				NULL, HFILL
			}
		},
		{
			&hf_wimax_ranging_symbol_offset,
			{
				"Ranging Symbol Offset", "wmx.cdma.ranging_symbol_offset",
				FT_UINT8, BASE_HEX, NULL, 0x0,
				NULL, HFILL
			}
		},
		{
			&hf_wimax_ranging_subchannel_offset,
			{
				"Ranging Sub-Channel Offset", "wmx.cdma.ranging_subchannel_offset",
				FT_UINT8, BASE_HEX, NULL, 0x0,
				NULL, HFILL
			}
		}
	};

	/* Setup protocol subtree array */
	static int *ett[] =
		{
			&ett_wimax_cdma_code_decoder,
		};

	proto_wimax_cdma_code_decoder = proto_register_protocol (
		"WiMax CDMA Code Attribute",   /* name       */
		"CDMA Code Attribute",   /* short name */
		"wmx.cdma"        /* abbrev     */
		);

	/* register the field display messages */
	proto_register_field_array(proto_wimax_cdma_code_decoder, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	register_dissector("wimax_cdma_code_burst_handler", dissect_wimax_cdma_code_decoder, proto_wimax_cdma_code_decoder);
}


/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
