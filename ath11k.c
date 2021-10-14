#include <errno.h>
#include <string.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

enum ath11k_tm_attr {
	__ATH11K_TM_ATTR_INVALID		= 0,
	ATH11K_TM_ATTR_CMD			= 1,
	ATH11K_TM_ATTR_DATA			= 2,
	ATH11K_TM_ATTR_WMI_CMDID		= 3,
	ATH11K_TM_ATTR_VERSION_MAJOR		= 4,
	ATH11K_TM_ATTR_VERSION_MINOR		= 5,
	ATH11K_TM_ATTR_WMI_OP_VERSION		= 6,

	/* keep last */
	__ATH11K_TM_ATTR_AFTER_LAST,
	ATH11K_TM_ATTR_MAX		= __ATH11K_TM_ATTR_AFTER_LAST - 1,
};

/* All ath11k testmode interface commands specified in
 * ATH11K_TM_ATTR_CMD
 */
enum ath11k_tm_cmd {
	/* Returns the supported ath11k testmode interface version in
	 * ATH11K_TM_ATTR_VERSION. Always guaranteed to work. User space
	 * uses this to verify it's using the correct version of the
	 * testmode interface
	 */
	ATH11K_TM_CMD_GET_VERSION = 0,

	/* The command used to transmit a WMI command to the firmware and
	 * the event to receive WMI events from the firmware. Without
	 * struct wmi_cmd_hdr header, only the WMI payload. Command id is
	 * provided with ATH11K_TM_ATTR_WMI_CMDID and payload in
	 * ATH11K_TM_ATTR_DATA.
	 */
	ATH11K_TM_CMD_WMI = 1,
};

#define WMITLV_TAG_ARRAY_UINT32 0x10
#define WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param 0x147

#define WMI_UNIT_TEST_CMDID 0x1f003

SECTION(ath11k);

static int print_ath11k_unittestcmd_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NL80211_ATTR_MAX + 1];
	struct nlattr *tb[ATH11K_TM_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	printf("print_ath11k_unittestcmd_handler:\n");

	if (!attrs[NL80211_ATTR_TESTDATA])
		return NL_SKIP;

	nla_parse(tb, ATH11K_TM_ATTR_MAX, nla_data(attrs[NL80211_ATTR_TESTDATA]),
		  nla_len(attrs[NL80211_ATTR_TESTDATA]), NULL);

	printf("ATH11K_TM_ATTR_CMD: %d\n", nla_get_u32(tb[ATH11K_TM_ATTR_CMD]));
	printf("ATH11K_TM_ATTR_WMI_CMDID: %d\n", nla_get_u32(tb[ATH11K_TM_ATTR_WMI_CMDID]));

	return NL_SKIP;
}



static int handle_ath11k_unittestcmd(struct nl80211_state *state,
			      struct nl_msg *msg, int argc, char **argv,
			      enum id_input id)
{
	struct {
		//uint32_t cmd_id; // XXX
		/* TLV tag and len; tag equals WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param  */
		uint32_t tlv_header;
		/* unique id identifying the VDEV, generated by the caller */
		uint32_t vdev_id;
		/* Identify the wlan module */
		uint32_t module_id;
		/* Num of test arguments passed */
		uint32_t num_args;
		/* unique id identifying the unit test cmd, generated by the caller */
		uint32_t diag_token;
		/**
		* TLV (tag length value) parameters follow the wmi_unit_test_cmd_fixed_param
		* structure. The TLV's are:
		*     A_UINT32 args[];
		**/
		uint32_t args_tlv_header;
		uint32_t args[100];
	} cmd;

	struct nlattr *tmdata;
	char *end;
	uint32_t i;

	cmd.tlv_header = WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param << 16 | (sizeof(uint32_t) * 4);
	cmd.vdev_id = 0; /* vdev id always 0 ?? */
	cmd.diag_token = 0;

	if (argc < 2)
		return 1;

	cmd.module_id = strtoul(argv[0], &end, 0);
	if (*end)
		return 1;

	cmd.num_args = strtoul(argv[1], &end, 0);
	if (*end)
		return 1;

	if ((uint32_t)argc != 2 + cmd.num_args)
		return 1;

	cmd.args_tlv_header = WMITLV_TAG_ARRAY_UINT32 << 16 | (sizeof(uint32_t) * cmd.num_args);
	for (i = 0; i < cmd.num_args; i++) {
		cmd.args[i] = strtoul(argv[2 + i], &end, 0);
		if (*end)
			return 1;
	}

	tmdata = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!tmdata)
		goto nla_put_failure;

	NLA_PUT_U32(msg, ATH11K_TM_ATTR_CMD, ATH11K_TM_CMD_WMI);
	NLA_PUT_U32(msg, ATH11K_TM_ATTR_WMI_CMDID, WMI_UNIT_TEST_CMDID);
	NLA_PUT(msg, ATH11K_TM_ATTR_DATA, (6 + cmd.num_args) * sizeof(uint32_t), &cmd);

	nla_nest_end(msg, tmdata);

	register_handler(print_ath11k_unittestcmd_handler, NULL);
	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(ath11k, unittestcmd, "<module id> <num args> <args>*", NL80211_CMD_TESTMODE, 0, CIB_PHY, handle_ath11k_unittestcmd, "");
