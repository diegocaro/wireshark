/* msg_dsa.c
 * WiMax MAC Management DSA-REQ/RSP/ACK Messages decoder
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

/*
#define DEBUG
*/

#include <epan/packet.h>
#include "wimax_mac.h"
#include "wimax_utils.h"

void proto_register_mac_mgmt_msg_dsa(void);
void proto_reg_handoff_mac_mgmt_msg_dsa(void);

static dissector_handle_t dsa_req_handle;
static dissector_handle_t dsa_rsp_handle;
static dissector_handle_t dsa_ack_handle;

static int proto_mac_mgmt_msg_dsa_decoder;
static int ett_mac_mgmt_msg_dsa_req_decoder;
static int ett_mac_mgmt_msg_dsa_rsp_decoder;
static int ett_mac_mgmt_msg_dsa_ack_decoder;

/* fix fields */
static int hf_dsa_transaction_id;
static int hf_dsa_confirmation_code;

static int dissect_mac_mgmt_msg_dsa_req_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	unsigned offset = 0;
	proto_item *dsa_item;
	proto_tree *dsa_tree;

	{	/* we are being asked for details */

		/* display MAC message type */
		dsa_item = proto_tree_add_protocol_format(tree, proto_mac_mgmt_msg_dsa_decoder, tvb, offset, -1,
							  "Dynamic Service Addition Request (DSA-REQ)");
		/* add MAC DSx subtree */
		dsa_tree = proto_item_add_subtree(dsa_item, ett_mac_mgmt_msg_dsa_req_decoder);
		/* Decode and display the Uplink Channel Descriptor (UCD) */
		/* display the Transaction ID */
		proto_tree_add_item(dsa_tree, hf_dsa_transaction_id, tvb, offset, 2, ENC_BIG_ENDIAN);
		/* move to next field */
		offset += 2;
		/* process DSA-REQ message TLV Encode Information */
		wimax_common_tlv_encoding_decoder(tvb_new_subset_remaining(tvb, offset), pinfo, dsa_tree);
	}
	return tvb_captured_length(tvb);
}

static int dissect_mac_mgmt_msg_dsa_rsp_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	unsigned offset = 0;
	proto_item *dsa_item;
	proto_tree *dsa_tree;

	{	/* we are being asked for details */
		/* display MAC message type */
		dsa_item = proto_tree_add_protocol_format(tree, proto_mac_mgmt_msg_dsa_decoder, tvb, offset, -1,
							"Dynamic Service Addition Response (DSA-RSP)");
		/* add MAC DSx subtree */
		dsa_tree = proto_item_add_subtree(dsa_item, ett_mac_mgmt_msg_dsa_rsp_decoder);
		/* Decode and display the Uplink Channel Descriptor (UCD) */
		/* display the Transaction ID */
		proto_tree_add_item(dsa_tree, hf_dsa_transaction_id, tvb, offset, 2, ENC_BIG_ENDIAN);
		/* move to next field */
		offset += 2;
		/* display the Confirmation Code */
		proto_tree_add_item(dsa_tree, hf_dsa_confirmation_code, tvb, offset, 1, ENC_BIG_ENDIAN);
		/* move to next field */
		offset++;
		/* process DSA RSP message TLV Encode Information */
		wimax_common_tlv_encoding_decoder(tvb_new_subset_remaining(tvb, offset), pinfo, dsa_tree);
	}
	return tvb_captured_length(tvb);
}

static int dissect_mac_mgmt_msg_dsa_ack_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	unsigned offset = 0;
	proto_item *dsa_item;
	proto_tree *dsa_tree;

	{	/* we are being asked for details */
		/* display MAC message type */
		dsa_item = proto_tree_add_protocol_format(tree, proto_mac_mgmt_msg_dsa_decoder, tvb, offset, -1,
							"Dynamic Service Addition Acknowledge (DSA-ACK)");
		/* add MAC DSx subtree */
		dsa_tree = proto_item_add_subtree(dsa_item, ett_mac_mgmt_msg_dsa_ack_decoder);
		/* Decode and display the Uplink Channel Descriptor (UCD) */
		/* display the Transaction ID */
		proto_tree_add_item(dsa_tree, hf_dsa_transaction_id, tvb, offset, 2, ENC_BIG_ENDIAN);
		/* move to next field */
		offset += 2;
		/* display the Confirmation Code */
		proto_tree_add_item(dsa_tree, hf_dsa_confirmation_code, tvb, offset, 1, ENC_BIG_ENDIAN);
		/* move to next field */
		offset++;
		/* process DSA-REQ message TLV Encode Information */
		wimax_common_tlv_encoding_decoder(tvb_new_subset_remaining(tvb, offset), pinfo, dsa_tree);
	}
	return tvb_captured_length(tvb);
}

/* Register Wimax Mac Payload Protocol and Dissector */
void proto_register_mac_mgmt_msg_dsa(void)
{
	/* DSx display */
	static hf_register_info hf[] =
	{
		{
			&hf_dsa_confirmation_code,
			{
				"Confirmation code", "wmx.dsa.confirmation_code",
				FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL
			}
		},
		{
			&hf_dsa_transaction_id,
			{
				"Transaction ID", "wmx.dsa.transaction_id",
				FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL
			}
		}
	};

	/* Setup protocol subtree array */
	static int *ett[] =
		{
			&ett_mac_mgmt_msg_dsa_req_decoder,
			&ett_mac_mgmt_msg_dsa_rsp_decoder,
			&ett_mac_mgmt_msg_dsa_ack_decoder,
		};

	proto_mac_mgmt_msg_dsa_decoder = proto_register_protocol (
		"WiMax DSA Messages", /* name       */
		"WiMax DSA",     /* short name */
		"wmx.dsa"        /* abbrev     */
		);

	proto_register_field_array(proto_mac_mgmt_msg_dsa_decoder, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	dsa_req_handle = register_dissector("mac_mgmt_msg_dsa_req_handler", dissect_mac_mgmt_msg_dsa_req_decoder, proto_mac_mgmt_msg_dsa_decoder);
	dsa_rsp_handle = register_dissector("mac_mgmt_msg_dsa_rsp_handler", dissect_mac_mgmt_msg_dsa_rsp_decoder, proto_mac_mgmt_msg_dsa_decoder);
	dsa_ack_handle = register_dissector("mac_mgmt_msg_dsa_ack_handler", dissect_mac_mgmt_msg_dsa_ack_decoder, proto_mac_mgmt_msg_dsa_decoder);
}

void
proto_reg_handoff_mac_mgmt_msg_dsa (void)
{
	dissector_add_uint("wmx.mgmtmsg", MAC_MGMT_MSG_DSA_REQ, dsa_req_handle);
	dissector_add_uint("wmx.mgmtmsg", MAC_MGMT_MSG_DSA_RSP, dsa_rsp_handle);
	dissector_add_uint("wmx.mgmtmsg", MAC_MGMT_MSG_DSA_ACK, dsa_ack_handle);
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
