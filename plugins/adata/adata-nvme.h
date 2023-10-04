#undef CMD_INC_FILE
#define CMD_INC_FILE plugins/adata/adata-nvme

#if !defined(ADATA_NVME) || defined(CMD_HEADER_MULTI_READ)
#define ADATA_NVME

#include "cmd.h"

PLUGIN(NAME("adata", "Adata vendor specific extensions", NVME_VERSION),
	COMMAND_LIST(
		ENTRY("get-info", "NVME ssd information", adata_get_info)
        ENTRY("create-ns", "Create and attach namespace", adata_create_ns)
		ENTRY("delete-ns", "Detach and delete namespace", adata_delete_ns)
	)
);

#endif

#include "define_cmd.h"