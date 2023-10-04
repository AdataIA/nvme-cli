#include <errno.h>
#include "nvme.h"
#include "nvme-wrap.h"
#include "nvme-print.h"

#include "common.h"

#define CREATE_CMD
#include "adata-nvme.h"

static int adata_get_info(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    char *desc = "Get nvme ssd info and namespace management capability.";
    struct nvme_id_ctrl ctrl;
    struct nvme_dev *dev;
    int err = 0;

    OPT_ARGS(opts) = {
		OPT_END()
	};

    err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
    {
		//printf("parse_and_open fail\n");
        goto ret;
    }

    err = nvme_cli_identify_ctrl(dev, &ctrl);
    if (err)
    {
        //printf("nvme_cli_identify_ctrl fail\n");
        goto close_dev;
    }
        
    //printf("identify controller success\n");

    printf("serial number : %-.*s\n", (int)sizeof(ctrl.sn), ctrl.sn);
    printf("model name    : %-.*s\n", (int)sizeof(ctrl.mn), ctrl.mn);

    __u16 nsm = (ctrl.oacs & 0x8) >> 3;
    if (nsm)
    {
        printf("Support namespace management\n");
    }
    else
    {
        printf("Does not support namespace management\n");
    }    
    
close_dev:
	dev_close(dev);
ret:
	return err;
}

// Do create and attach ns in a fucntion
static int adata_create_ns(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
    char *desc = "Create namespace and attach.";
	const char *flbas =
	    "Formatted LBA size (FLBAS), if entering this value ignore \'block-size\' field";
	const char *dps = "data protection settings (DPS)";
	const char *nmic = "multipath and sharing capabilities (NMIC)";
	const char *anagrpid = "ANA Group Identifier (ANAGRPID)";
	const char *nvmsetid = "NVM Set Identifier (NVMSETID)";
	const char *endgid = "Endurance Group Identifier (ENDGID)";
	const char *csi = "command set identifier (CSI)";
	const char *lbstm = "logical block storage tag mask (LBSTM)";
	const char *nphndls = "Number of Placement Handles (NPHNDLS)";
	const char *bs = "target block size, specify only if \'FLBAS\' value not entered";
	const char *timeout = "timeout value, in milliseconds";
	const char *nsze_si = "size of ns (NSZE) in standard SI units";
	const char *ncap_si = "capacity of ns (NCAP) in standard SI units";
	const char *azr = "Allocate ZRWA Resources (AZR) for Zoned Namespace Command Set";
	const char *rar = "Requested Active Resources (RAR) for Zoned Namespace Command Set";
	const char *ror = "Requested Open Resources (ROR) for Zoned Namespace Command Set";
	const char *rnumzrwa =
	    "Requested Number of ZRWA Resources (RNUMZRWA) for Zoned Namespace Command Set";
	const char *phndls = "Comma separated list of Placement Handle Associated RUH";
	const char *cont = "optional comma-sep controller id list";

    struct nvme_id_ns ns;
	struct nvme_dev *dev;
	int err = 0, i;
	__u32 nsid;
	uint16_t num_phandle;
	uint16_t phndl[128] = { 0, };
	int num, list[2048];
	struct nvme_ctrl_list cntlist;
	__u16 ctrlist[2048];

    struct config {
		__u64	nsze;
		__u64	ncap;
		__u8	flbas;
		__u8	dps;
		__u8	nmic;
		__u32	anagrpid;
		__u16	nvmsetid;
		__u16	endgid;
		__u64	bs;
		__u32	timeout;
		__u8	csi;
		__u64	lbstm;
		__u16	nphndls;
		char	*nsze_si;
		char	*ncap_si;
		bool	azr;
		__u32	rar;
		__u32	ror;
		__u32	rnumzrwa;
		char	*phndls;
		char	*cntlist;
	};

	struct config cfg = {
		.nsze		= 0,
		.ncap		= 0,
		.flbas		= 0xff,
		.dps		= 0,
		.nmic		= 0,
		.anagrpid	= 0,
		.nvmsetid	= 0,
		.endgid		= 0,
		.bs			= 0x00,
		.timeout	= 120000,
		.csi		= 0,
		.lbstm		= 0,
		.nphndls	= 0,
		.nsze_si	= NULL,
		.ncap_si	= NULL,
		.azr		= false,
		.rar		= 0,
		.ror		= 0,
		.rnumzrwa	= 0,
		.phndls		= "",
		.cntlist	= "",
	};

    OPT_ARGS(opts) = {
		OPT_BYTE("flbas",        'f', &cfg.flbas,    flbas),
		OPT_BYTE("dps",          'd', &cfg.dps,      dps),
		OPT_BYTE("nmic",         'm', &cfg.nmic,     nmic),
		OPT_UINT("anagrp-id",    'a', &cfg.anagrpid, anagrpid),
		OPT_UINT("nvmset-id",    'i', &cfg.nvmsetid, nvmsetid),
		OPT_UINT("endg-id",      'e', &cfg.endgid,   endgid),
		OPT_SUFFIX("block-size", 'b', &cfg.bs,       bs),
		OPT_UINT("timeout",      't', &cfg.timeout,  timeout),
		OPT_BYTE("csi",          'y', &cfg.csi,      csi),
		OPT_SUFFIX("lbstm",      'l', &cfg.lbstm,    lbstm),
		OPT_SHRT("nphndls",      'n', &cfg.nphndls,  nphndls),
		OPT_STR("nsze-si",       'S', &cfg.nsze_si,  nsze_si),
		OPT_STR("ncap-si",       'C', &cfg.ncap_si,  ncap_si),
		OPT_FLAG("azr",          'z', &cfg.azr,      azr),
		OPT_UINT("rar",          'r', &cfg.rar,      rar),
		OPT_UINT("ror",          'o', &cfg.ror,      ror),
		OPT_UINT("rnumzrwa",     'u', &cfg.rnumzrwa, rnumzrwa),
		OPT_LIST("phndls",       'p', &cfg.phndls,   phndls),
		OPT_LIST("controllers",  'c', &cfg.cntlist,  cont),
		OPT_END()
	};

    err = parse_and_open(&dev, argc, argv, desc, opts);
    if (err)
    {
        //printf("parse_and_open fail\n");
        goto ret;
    }
	
	if (cfg.flbas != 0xff && cfg.bs != 0x00) {
		nvme_show_error(
		    "Invalid specification of both FLBAS and Block Size, please specify only one");
		err = -EINVAL;
		goto close_dev;
	}
	if (cfg.bs) {
		if ((cfg.bs & (~cfg.bs + 1)) != cfg.bs) {
			nvme_show_error(
			    "Invalid value for block size (%"PRIu64"). Block size must be a power of two",
			    (uint64_t)cfg.bs);
			err = -EINVAL;
			goto close_dev;
		}
		err = nvme_cli_identify_ns(dev, NVME_NSID_ALL, &ns);
		if (err) {
			if (err < 0) {
				nvme_show_error("identify-namespace: %s", nvme_strerror(errno));
			} else {
				fprintf(stderr, "identify failed\n");
				nvme_show_status(err);
			}
			goto close_dev;
		}
		for (i = 0; i <= ns.nlbaf; ++i) {
			if ((1 << ns.lbaf[i].ds) == cfg.bs && ns.lbaf[i].ms == 0) {
				cfg.flbas = i;
				break;
			}
		}

	}
	if (cfg.flbas == 0xff) {
		fprintf(stderr, "FLBAS corresponding to block size %"PRIu64" not found\n",
			(uint64_t)cfg.bs);
		fprintf(stderr, "Please correct block size, or specify FLBAS directly\n");

		err = -EINVAL;
		goto close_dev;
	}

	err = __parse_lba_num_si(dev, "nsze", cfg.nsze_si, cfg.flbas, &cfg.nsze);
	if (err)
		goto close_dev;

	err = __parse_lba_num_si(dev, "ncap", cfg.ncap_si, cfg.flbas, &cfg.ncap);
	if (err)
		goto close_dev;

	if (cfg.csi != NVME_CSI_ZNS && (cfg.azr || cfg.rar || cfg.ror || cfg.rnumzrwa)) {
		nvme_show_error("Invalid ZNS argument is given (CSI:%#x)", cfg.csi);
		err = -EINVAL;
		goto close_dev;
	}

	struct nvme_ns_mgmt_host_sw_specified data = {
		.nsze = cpu_to_le64(cfg.nsze),
		.ncap = cpu_to_le64(cfg.ncap),
		.flbas = cfg.flbas,
		.dps = cfg.dps,
		.nmic = cfg.nmic,
		.anagrpid = cpu_to_le32(cfg.anagrpid),
		.nvmsetid = cpu_to_le16(cfg.nvmsetid),
		.endgid = cpu_to_le16(cfg.endgid),
		.lbstm = cpu_to_le64(cfg.lbstm),
		.zns.znsco = cfg.azr,
		.zns.rar = cpu_to_le32(cfg.rar),
		.zns.ror = cpu_to_le32(cfg.ror),
		.zns.rnumzrwa = cpu_to_le32(cfg.rnumzrwa),
		.nphndls = cpu_to_le16(cfg.nphndls),
	};

	num_phandle = argconfig_parse_comma_sep_array_short(cfg.phndls, phndl, ARRAY_SIZE(phndl));
	if (cfg.nphndls != num_phandle) {
		nvme_show_error("Invalid Placement handle list");
		err = -EINVAL;
		goto close_dev;
	}

	for (i = 0; i < num_phandle; i++)
		data.phndl[i] = cpu_to_le16(phndl[i]);

	// create ns
	err = nvme_cli_ns_mgmt_create(dev, &data, &nsid, cfg.timeout, cfg.csi);
	if (!err)
		printf("%s: Success, created nsid:%d\n", "create ns", nsid);
	else if (err > 0)
		nvme_show_status(err);
	else
		nvme_show_error("create namespace: %s", nvme_strerror(errno));

	// attach ns
	if (!nsid) {
		nvme_show_error("%s: namespace-id parameter required", cmd->name);
		err = -EINVAL;
		goto close_dev;
	}

	num = argconfig_parse_comma_sep_array(cfg.cntlist, list, 2047);
	if (!num)
		fprintf(stderr, "warning: empty controller-id list will result in no actual change in namespace attachment\n");

	if (num == -1) {
		nvme_show_error("%s: controller id list is malformed", cmd->name);
		err = -EINVAL;
		goto close_dev;
	}

	for (i = 0; i < num; i++)
		ctrlist[i] = (__u16)list[i];

	nvme_init_ctrl_list(&cntlist, num, ctrlist);

	err = nvme_cli_ns_attach_ctrls(dev, nsid, &cntlist);

	if (!err)
		printf("%s: Success, nsid:%d\n", "attach ns", nsid);
	else if (err > 0)
		nvme_show_status(err);
	else
		nvme_show_perror("attach namespace");
	
close_dev:
	dev_close(dev);
ret:
	return err;
}

static int adata_delete_ns(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	char *desc = "Detach and delete the given namespace.";
	const char *namespace_id = "namespace to delete";
	const char *cont = "optional comma-sep controller id list";
	struct nvme_dev *dev;
	int err, num, i, list[2048];
	struct nvme_ctrl_list cntlist;
	__u16 ctrlist[2048];

	struct config {
		__u32	namespace_id;
		__u32	timeout;
		char	*cntlist;
	};

	struct config cfg = {
		.namespace_id	= 0,
		.timeout	= 120000,
		.cntlist	= "",
	};

	OPT_ARGS(opts) = {
		OPT_UINT("namespace-id", 'n', &cfg.namespace_id, namespace_id),
		OPT_LIST("controllers",  'c', &cfg.cntlist,      cont),
		OPT_END()
	};

	err = parse_and_open(&dev, argc, argv, desc, opts);
    if (err)
    {
        //printf("parse_and_open fail\n");
        goto ret;
    }

	if (!cfg.namespace_id)
	{
		nvme_show_error("%s: namespace-id parameter required", cmd->name);
		err = -EINVAL;
		goto close_dev;
	}

	num = argconfig_parse_comma_sep_array(cfg.cntlist, list, 2047);
	if (!num)
		fprintf(stderr, "warning: empty controller-id list will result in no actual change in namespace attachment\n");

	if (num == -1) {
		nvme_show_error("%s: controller id list is malformed", cmd->name);
		err = -EINVAL;
		goto close_dev;
	}

	for (i = 0; i < num; i++)
		ctrlist[i] = (__u16)list[i];

	nvme_init_ctrl_list(&cntlist, num, ctrlist);
	
	// Do detach first
	err = nvme_cli_ns_detach_ctrls(dev, cfg.namespace_id, &cntlist);
	if (!err)
		printf("detach ns: Success, nsid:%d\n", cfg.namespace_id);
	else if (err > 0)
		nvme_show_status(err);
	else
		nvme_show_perror("detach namespace");

	// Delete ns
	err = nvme_cli_ns_mgmt_delete(dev, cfg.namespace_id);
	if (!err)
		printf("delete ns: Success, deleted nsid:%d\n", cfg.namespace_id);
	else if (err > 0)
		nvme_show_status(err);
	else
		nvme_show_error("delete namespace: %s", nvme_strerror(errno));

close_dev:
	dev_close(dev);
ret:
	return err;
}