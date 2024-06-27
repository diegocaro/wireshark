/* DO NOT EDIT
	This file was automatically generated by Pidl
	from initshutdown.idl and initshutdown.cnf.

	Pidl is a perl based IDL compiler for DCE/RPC idl files.
	It is maintained by the Samba team, not the Wireshark team.
	Instructions on how to download and install Pidl can be
	found at https://gitlab.com/wireshark/wireshark/-/wikis/Pidl
*/

#include "packet-dcerpc-lsa.h"

#ifndef __PACKET_DCERPC_INITSHUTDOWN_H
#define __PACKET_DCERPC_INITSHUTDOWN_H

#define SHTDN_REASON_MAJOR_OTHER (0x00000000)
#define SHTDN_REASON_MAJOR_HARDWARE (0x00010000)
#define SHTDN_REASON_MAJOR_OPERATINGSYSTEM (0x00020000)
#define SHTDN_REASON_MAJOR_SOFTWARE (0x00030000)
#define SHTDN_REASON_MAJOR_APPLICATION (0x00040000)
#define SHTDN_REASON_MAJOR_SYSTEM (0x00050000)
#define SHTDN_REASON_MAJOR_POWER (0x00060000)
#define SHTDN_REASON_MAJOR_LEGACY_API (0x00070000)
extern const value_string initshutdown_initshutdown_ReasonMajor_vals[];
int initshutdown_dissect_enum_ReasonMajor(tvbuff_t *tvb _U_, int offset _U_, packet_info *pinfo _U_, proto_tree *tree _U_, dcerpc_info* di _U_, uint8_t *drep _U_, int hf_index _U_, guint32 *param _U_);
#define SHTDN_REASON_MINOR_OTHER (0x00000000)
#define SHTDN_REASON_MINOR_MAINTENANCE (0x00000001)
#define SHTDN_REASON_MINOR_INSTALLATION (0x00000002)
#define SHTDN_REASON_MINOR_UPGRADE (0x00000003)
#define SHTDN_REASON_MINOR_RECONFIG (0x00000004)
#define SHTDN_REASON_MINOR_HUNG (0x00000005)
#define SHTDN_REASON_MINOR_UNSTABLE (0x00000006)
#define SHTDN_REASON_MINOR_DISK (0x00000007)
#define SHTDN_REASON_MINOR_PROCESSOR (0x00000008)
#define SHTDN_REASON_MINOR_NETWORKCARD (0x00000009)
#define SHTDN_REASON_MINOR_POWER_SUPPLY (0x0000000a)
#define SHTDN_REASON_MINOR_CORDUNPLUGGED (0x0000000b)
#define SHTDN_REASON_MINOR_ENVIRONMENT (0x0000000c)
#define SHTDN_REASON_MINOR_HARDWARE_DRIVER (0x0000000d)
#define SHTDN_REASON_MINOR_OTHERDRIVER (0x0000000e)
#define SHTDN_REASON_MINOR_BLUESCREEN (0x0000000f)
#define SHTDN_REASON_MINOR_SERVICEPACK (0x00000010)
#define SHTDN_REASON_MINOR_HOTFIX (0x00000011)
#define SHTDN_REASON_MINOR_SECURITYFIX (0x00000012)
#define SHTDN_REASON_MINOR_SECURITY (0x00000013)
#define SHTDN_REASON_MINOR_NETWORK_CONNECTIVITY (0x00000014)
#define SHTDN_REASON_MINOR_WMI (0x00000015)
#define SHTDN_REASON_MINOR_SERVICEPACK_UNINSTALL (0x00000016)
#define SHTDN_REASON_MINOR_HOTFIX_UNINSTALL (0x00000017)
#define SHTDN_REASON_MINOR_SECURITYFIX_UNINSTALL (0x00000018)
#define SHTDN_REASON_MINOR_MMC (0x00000019)
#define SHTDN_REASON_MINOR_TERMSRV (0x00000020)
extern const value_string initshutdown_initshutdown_ReasonMinor_vals[];
int initshutdown_dissect_enum_ReasonMinor(tvbuff_t *tvb _U_, int offset _U_, packet_info *pinfo _U_, proto_tree *tree _U_, dcerpc_info* di _U_, uint8_t *drep _U_, int hf_index _U_, guint32 *param _U_);
int initshutdown_dissect_bitmap_ReasonFlags(tvbuff_t *tvb _U_, int offset _U_, packet_info *pinfo _U_, proto_tree *tree _U_, dcerpc_info* di _U_, uint8_t *drep _U_, int hf_index _U_, uint32_t param _U_);
#endif /* __PACKET_DCERPC_INITSHUTDOWN_H */
