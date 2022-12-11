#include <rte_eal.h>
#include <rte_acl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


struct ipv4_5tuple{
	uint8_t proto;
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
};

struct rte_acl_field_def ipv4_defs[5] = {
		{
			.type = RTE_ACL_FIELD_TYPE_BITMASK,
			.size = sizeof(uint8_t),
			.field_index = 0,
			.input_index = 0,
			.offset = offsetof(struct ipv4_5tuple, proto),
		},
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = 1,
			.input_index = 1,
			.offset = offsetof(struct ipv4_5tuple, src_ip),
		},	
		{
			.type = RTE_ACL_FIELD_TYPE_MASK,
			.size = sizeof(uint32_t),
			.field_index = 2,
			.input_index = 2,
			.offset = offsetof(struct ipv4_5tuple, dst_ip),
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = 3,
			.input_index = 3,
			.offset = offsetof(struct ipv4_5tuple, src_port),
		},
		{
			.type = RTE_ACL_FIELD_TYPE_RANGE,
			.size = sizeof(uint16_t),
			.field_index = 4,
			.input_index = 3,
			.offset = offsetof(struct ipv4_5tuple, dst_port),
		},

};


RTE_ACL_RULE_DEF(acl_ipv4_rule, RTE_DIM(ipv4_defs));

/** Create IPv4 address */
#define IPv4(a, b, c, d) ((uint32_t)(((a) & 0xff) << 24) | \
		(((b) & 0xff) << 16) | \
		(((c) & 0xff) << 8)  | \
		((d) & 0xff))


int main(int argc, void **argv)
{
	struct rte_acl_ctx *actx;
	struct rte_acl_config cfg;
	uint32_t results;
	uint8_t *pdata[2] = {0};
	struct ipv4_5tuple data[2] = {0};
	int ret;
	
	struct rte_acl_param acl_param = {
		.name = "my_acl_ctx",
		.socket_id = SOCKET_ID_ANY,
		.rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ipv4_defs)),
		.max_rule_num = 0x40,
	};

	ret = rte_eal_init(argc, argv);
	if (0 != ret){
		printf("rte eal init failed \n");
		return -1;
	}

	/* create an empty AC context  */
	actx = rte_acl_create(&acl_param);
	if (actx == NULL){
		 printf("rte acl create failed!\n");
		 return -1;
	}

	
	struct acl_ipv4_rule acl_rules[] = {
		{
			.data = {.userdata = 1, .category_mask = 1, .priority = 1},
			.field[2] = {.value.u32 = IPv4(1,2,0,0), .mask_range.u32 = 16},
		},
	};
	
	/* add rules to the context */
	ret = rte_acl_add_rules(actx,acl_rules,RTE_DIM(acl_rules));
	if (0 != ret){
		printf("add acl add rules failed! %d\n", ret);
		return -1;
	}

	/* build the runtime structures for added rules, with  categories. */
	cfg.num_categories = 1;
	cfg.num_fields = RTE_DIM(ipv4_defs);
	memcpy(cfg.defs, ipv4_defs, sizeof(ipv4_defs));
	ret = rte_acl_build(actx, &cfg);
	if (0 != ret){
		printf("acl build failed!\n");
		return -1;
	}

	data[0].dst_ip = rte_be_to_cpu_32(0x01020304);
	pdata[0] = &data[0];
	rte_acl_classify(actx, pdata, &results, 1, 1);

	printf("find results %d\n", results);
	
	return 0;
}

