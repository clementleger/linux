// SPDX-License-Identifier: GPL-2.0
/**
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 Kalray Inc.
 */

#include <linux/module.h>
#include <linux/module.h>

#include "kvx-net.h"

#define STR_LEN 20

#define DECLARE_SYSFS_ENTRY(s) \
struct sysfs_##s##_entry { \
	struct attribute attr; \
	ssize_t (*show)(struct kvx_eth_##s *p, char *buf); \
	ssize_t (*store)(struct kvx_eth_##s *p, const char *buf, size_t s); \
}; \
static ssize_t s##_attr_show(struct kobject *kobj, \
			     struct attribute *attr, char *buf) \
{ \
	struct sysfs_##s##_entry *entry = container_of(attr, \
					 struct sysfs_##s##_entry, attr); \
	struct kvx_eth_##s *p = container_of(kobj, struct kvx_eth_##s, kobj); \
	if (!entry->show) \
		return -EIO; \
	return entry->show(p, buf); \
} \
static ssize_t s##_attr_store(struct kobject *kobj, \
			struct attribute *attr, const char *buf, size_t count) \
{ \
	struct sysfs_##s##_entry *entry = container_of(attr, \
					 struct sysfs_##s##_entry, attr); \
	struct kvx_eth_##s *p = container_of(kobj, struct kvx_eth_##s, kobj); \
	if (!entry->store) \
		return -EIO; \
	return entry->store(p, buf, count); \
}

#define SYSFS_TYPES(s) \
const struct sysfs_ops s##_sysfs_ops = { \
	.show  = s##_attr_show, \
	.store = s##_attr_store, \
}; \
struct kobj_type s##_ktype = { \
	.sysfs_ops = &s##_sysfs_ops, \
	.default_attrs = s##_attrs, \
}

#define FIELD_RW_ENTRY(s, f, min, max) \
static ssize_t s##_##f##_show(struct kvx_eth_##s *p, char *buf) \
{ \
	if (p->update) \
		p->update(p); \
	return scnprintf(buf, STR_LEN, "%i\n", p->f); \
} \
static ssize_t s##_##f##_store(struct kvx_eth_##s *p, const char *buf, \
		size_t count) \
{ \
	ssize_t ret; \
	unsigned int val; \
	ret = kstrtouint(buf, 0, &val); \
	if (ret) \
		return ret; \
	if (val < min || val > max) \
		return -EINVAL; \
	p->f = val; \
	kvx_eth_##s##_cfg(p->hw, p); \
	return count; \
} \
static struct sysfs_##s##_entry s##_##f##_attr = __ATTR(f, 0644, \
		s##_##f##_show, s##_##f##_store) \

#define FIELD_R_ENTRY(s, f, min, max) \
static ssize_t s##_##f##_show(struct kvx_eth_##s *p, char *buf) \
{ \
	if (p->update) \
		p->update(p); \
	return scnprintf(buf, STR_LEN, "%i\n", p->f); \
} \
static struct sysfs_##s##_entry s##_##f##_attr = __ATTR(f, 0444, \
		s##_##f##_show, NULL)

DECLARE_SYSFS_ENTRY(mac_f);
FIELD_RW_ENTRY(mac_f, loopback_mode, 0, MAC_RX2TX_LOOPBACK);
FIELD_RW_ENTRY(mac_f, tx_fcs_offload, 0, 1);
FIELD_R_ENTRY(mac_f, pfc_mode, 0, MAC_PAUSE);

static struct attribute *mac_f_attrs[] = {
	&mac_f_loopback_mode_attr.attr,
	&mac_f_tx_fcs_offload_attr.attr,
	&mac_f_pfc_mode_attr.attr,
	NULL,
};
SYSFS_TYPES(mac_f);

DECLARE_SYSFS_ENTRY(phy_f);
FIELD_RW_ENTRY(phy_f, bert_en, 0, 1);
static struct attribute *phy_f_attrs[] = {
	&phy_f_bert_en_attr.attr,
	NULL,
};
SYSFS_TYPES(phy_f);

DECLARE_SYSFS_ENTRY(phy_param);
FIELD_RW_ENTRY(phy_param, pre, 0, 32);
FIELD_RW_ENTRY(phy_param, post, 0, 32);
FIELD_RW_ENTRY(phy_param, swing, 0, 32);
FIELD_RW_ENTRY(phy_param, trig_rx_adapt, 0, 1);
FIELD_RW_ENTRY(phy_param, en, 0, 1);
FIELD_R_ENTRY(phy_param, fom, 0, U8_MAX);

static struct attribute *phy_param_attrs[] = {
	&phy_param_pre_attr.attr,
	&phy_param_post_attr.attr,
	&phy_param_swing_attr.attr,
	&phy_param_fom_attr.attr,
	&phy_param_trig_rx_adapt_attr.attr,
	&phy_param_en_attr.attr,
	NULL,
};
SYSFS_TYPES(phy_param);

DECLARE_SYSFS_ENTRY(rx_bert_param);
FIELD_RW_ENTRY(rx_bert_param, err_cnt, 0, U32_MAX);
FIELD_RW_ENTRY(rx_bert_param, sync, 0, 1);
FIELD_RW_ENTRY(rx_bert_param, rx_mode, BERT_DISABLED, BERT_MODE_NB);

static struct attribute *rx_bert_param_attrs[] = {
	&rx_bert_param_err_cnt_attr.attr,
	&rx_bert_param_sync_attr.attr,
	&rx_bert_param_rx_mode_attr.attr,
	NULL,
};
SYSFS_TYPES(rx_bert_param);

DECLARE_SYSFS_ENTRY(tx_bert_param);
FIELD_RW_ENTRY(tx_bert_param, trig_err, 0, 1);
FIELD_RW_ENTRY(tx_bert_param, pat0, 0, U16_MAX);
FIELD_RW_ENTRY(tx_bert_param, tx_mode, BERT_DISABLED, BERT_MODE_NB);

static struct attribute *tx_bert_param_attrs[] = {
	&tx_bert_param_trig_err_attr.attr,
	&tx_bert_param_pat0_attr.attr,
	&tx_bert_param_tx_mode_attr.attr,
	NULL,
};
SYSFS_TYPES(tx_bert_param);

DECLARE_SYSFS_ENTRY(lb_f);
FIELD_RW_ENTRY(lb_f, default_dispatch_policy,
	       0, DEFAULT_DISPATCH_POLICY_NB);
FIELD_RW_ENTRY(lb_f, keep_all_crc_error_pkt, 0, 1);
FIELD_RW_ENTRY(lb_f, store_and_forward, 0, 1);
FIELD_RW_ENTRY(lb_f, add_header, 0, 1);
FIELD_RW_ENTRY(lb_f, add_footer, 0, 1);
FIELD_R_ENTRY(lb_f, drop_mtu_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, drop_fcs_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, drop_crc_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, drop_rule_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, drop_fifo_overflow_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, drop_total_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, default_hit_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, global_drop_cnt, 0, U32_MAX);
FIELD_R_ENTRY(lb_f, global_no_pfc_drop_cnt, 0, U32_MAX);

static struct attribute *lb_f_attrs[] = {
	&lb_f_default_dispatch_policy_attr.attr,
	&lb_f_keep_all_crc_error_pkt_attr.attr,
	&lb_f_store_and_forward_attr.attr,
	&lb_f_add_header_attr.attr,
	&lb_f_add_footer_attr.attr,
	&lb_f_drop_mtu_cnt_attr.attr,
	&lb_f_drop_fcs_cnt_attr.attr,
	&lb_f_drop_crc_cnt_attr.attr,
	&lb_f_drop_rule_cnt_attr.attr,
	&lb_f_drop_fifo_overflow_cnt_attr.attr,
	&lb_f_drop_total_cnt_attr.attr,
	&lb_f_default_hit_cnt_attr.attr,
	&lb_f_global_drop_cnt_attr.attr,
	&lb_f_global_no_pfc_drop_cnt_attr.attr,
	NULL,
};
SYSFS_TYPES(lb_f);

DECLARE_SYSFS_ENTRY(rx_noc);
FIELD_RW_ENTRY(rx_noc, vchan0_pps_timer, 0, U16_MAX);
FIELD_RW_ENTRY(rx_noc, vchan0_payload_flit_nb, 0, 0xF);
FIELD_RW_ENTRY(rx_noc, vchan1_pps_timer, 0, U16_MAX);
FIELD_RW_ENTRY(rx_noc, vchan1_payload_flit_nb, 0, 0xF);

static struct attribute *rx_noc_attrs[] = {
	&rx_noc_vchan0_pps_timer_attr.attr,
	&rx_noc_vchan0_payload_flit_nb_attr.attr,
	&rx_noc_vchan1_pps_timer_attr.attr,
	&rx_noc_vchan1_payload_flit_nb_attr.attr,
	NULL,
};
SYSFS_TYPES(rx_noc);

DECLARE_SYSFS_ENTRY(lut_f);
FIELD_RW_ENTRY(lut_f, qpn_enable, 0, RX_LB_LUT_QPN_CTRL_QPN_EN_MASK);
FIELD_RW_ENTRY(lut_f, lane_enable, 0, 1);
FIELD_RW_ENTRY(lut_f, rule_enable, 0, 1);
FIELD_RW_ENTRY(lut_f, pfc_enable, 0, 1);

static struct attribute *lut_f_attrs[] = {
	&lut_f_qpn_enable_attr.attr,
	&lut_f_lane_enable_attr.attr,
	&lut_f_rule_enable_attr.attr,
	&lut_f_pfc_enable_attr.attr,
	NULL,
};
SYSFS_TYPES(lut_f);

DECLARE_SYSFS_ENTRY(pfc_f);
FIELD_RW_ENTRY(pfc_f, global_release_level, 0,
	       RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(pfc_f, global_drop_level, 0, RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(pfc_f, global_alert_level, 0,
	       RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(pfc_f, global_pfc_en, 0, 1);
FIELD_RW_ENTRY(pfc_f, global_pause_en, 0, 1);

static struct attribute *pfc_f_attrs[] = {
	&pfc_f_global_release_level_attr.attr,
	&pfc_f_global_drop_level_attr.attr,
	&pfc_f_global_alert_level_attr.attr,
	&pfc_f_global_pfc_en_attr.attr,
	&pfc_f_global_pause_en_attr.attr,
	NULL,
};
SYSFS_TYPES(pfc_f);

DECLARE_SYSFS_ENTRY(tx_f);
FIELD_RW_ENTRY(tx_f, header_en, 0, 1);
FIELD_RW_ENTRY(tx_f, drop_en, 0, 1);
FIELD_RW_ENTRY(tx_f, nocx_en, 0, 1);
FIELD_RW_ENTRY(tx_f, nocx_pack_en, 0, 1);
FIELD_RW_ENTRY(tx_f, pfc_en, 0, 1);
FIELD_RW_ENTRY(tx_f, pause_en, 0, 1);
FIELD_RW_ENTRY(tx_f, rr_trigger, 0, 0xF);
FIELD_RW_ENTRY(tx_f, lane_id, 0, KVX_ETH_LANE_NB - 1);
FIELD_R_ENTRY(tx_f, drop_cnt, 0, U32_MAX);
FIELD_R_ENTRY(tx_f, fifo_level, 0, U32_MAX);
FIELD_R_ENTRY(tx_f, xoff, 0, 1);

static struct attribute *tx_f_attrs[] = {
	&tx_f_header_en_attr.attr,
	&tx_f_drop_en_attr.attr,
	&tx_f_nocx_en_attr.attr,
	&tx_f_nocx_pack_en_attr.attr,
	&tx_f_pfc_en_attr.attr,
	&tx_f_pause_en_attr.attr,
	&tx_f_rr_trigger_attr.attr,
	&tx_f_lane_id_attr.attr,
	&tx_f_drop_cnt_attr.attr,
	&tx_f_fifo_level_attr.attr,
	&tx_f_xoff_attr.attr,
	NULL,
};
SYSFS_TYPES(tx_f);

DECLARE_SYSFS_ENTRY(cl_f);
FIELD_RW_ENTRY(cl_f, quanta, 0, DEFAULT_PAUSE_QUANTA);
FIELD_RW_ENTRY(cl_f, release_level, 0, RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(cl_f, drop_level, 0, RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(cl_f, alert_level, 0, RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK);
FIELD_RW_ENTRY(cl_f, pfc_ena, 0, 1);

static struct attribute *cl_f_attrs[] = {
	&cl_f_quanta_attr.attr,
	&cl_f_release_level_attr.attr,
	&cl_f_drop_level_attr.attr,
	&cl_f_alert_level_attr.attr,
	&cl_f_pfc_ena_attr.attr,
	NULL,
};
SYSFS_TYPES(cl_f);

DECLARE_SYSFS_ENTRY(dt_f);
FIELD_RW_ENTRY(dt_f, cluster_id, 0, 0xFF);
FIELD_RW_ENTRY(dt_f, rx_channel, 0, KVX_ETH_RX_TAG_NB - 1);
FIELD_RW_ENTRY(dt_f, split_trigger, 0, 0x7F);
FIELD_RW_ENTRY(dt_f, vchan, 0, 1);

static struct attribute *dt_f_attrs[] = {
	&dt_f_cluster_id_attr.attr,
	&dt_f_rx_channel_attr.attr,
	&dt_f_split_trigger_attr.attr,
	&dt_f_vchan_attr.attr,
	NULL,
};
SYSFS_TYPES(dt_f);

DECLARE_SYSFS_ENTRY(parser_f);
FIELD_R_ENTRY(parser_f, enable, 0, 1);
static struct attribute *parser_f_attrs[] = {
	&parser_f_enable_attr.attr,
	NULL,
};

SYSFS_TYPES(parser_f);

DECLARE_SYSFS_ENTRY(rule_f);
FIELD_R_ENTRY(rule_f, enable, 0, 1);
FIELD_R_ENTRY(rule_f, type, 0, 0x1F);
FIELD_R_ENTRY(rule_f, add_metadata_index, 0, 1);
FIELD_R_ENTRY(rule_f, check_header_checksum, 0, 1);

static struct attribute *rule_f_attrs[] = {
	&rule_f_enable_attr.attr,
	&rule_f_type_attr.attr,
	&rule_f_add_metadata_index_attr.attr,
	&rule_f_check_header_checksum_attr.attr,
	NULL,
};
SYSFS_TYPES(rule_f);

/**
 * struct sysfs_type
 * @name: sysfs entry name
 * @offset: kobj offset
 * @type: ops and attributes definition
 */
struct sysfs_type {
	char *name;
	int   offset;
	void *type;
};

static const struct sysfs_type t[] = {
	{.name = "mac", .offset = offsetof(struct kvx_eth_lane_cfg, mac_f.kobj),
		.type = &mac_f_ktype },
	{.name = "pfc", .offset = offsetof(struct kvx_eth_lane_cfg, pfc_f.kobj),
		.type = &pfc_f_ktype },
};

static int kvx_eth_kobject_add(struct net_device *netdev,
			       struct kvx_eth_lane_cfg *cfg,
			       const struct sysfs_type *t)
{
	struct kobject *kobj = (struct kobject *)((char *)cfg + t->offset);
	int ret = 0;

	ret = kobject_init_and_add(kobj, t->type, &netdev->dev.kobj, t->name);
	if (ret) {
		netdev_warn(netdev, "Sysfs init error (%d)\n", ret);
		kobject_put(kobj);
	}
	return ret;
}

static void kvx_eth_kobject_del(struct kvx_eth_lane_cfg *cfg,
				const struct sysfs_type *t)
{
	struct kobject *kobj = (struct kobject *)((char *)cfg + t->offset);

	kobject_del(kobj);
	kobject_put(kobj);
}

static struct kset *lb_kset;
static struct kset *rx_noc_kset;
static struct kset *tx_kset;
static struct kset *dt_kset;
static struct kset *parser_kset;
static struct kset *rule_kset;
static struct kset *pfc_cl_kset;
static struct kset *phy_param_kset;
static struct kset *rx_bert_param_kset;
static struct kset *tx_bert_param_kset;

#define kvx_declare_kset(s, name) \
int kvx_kset_##s##_create(struct kvx_eth_netdev *ndev, struct kobject *pkobj, \
			  struct kset *k, struct kvx_eth_##s *p, size_t size) \
{ \
	struct kvx_eth_##s *f; \
	int i, j, ret = 0; \
	k = kset_create_and_add(name, NULL, pkobj); \
	if (!k) { \
		pr_err(#name" sysfs kobject registration failed\n"); \
		return -EINVAL; \
	} \
	for (i = 0; i < size; ++i) { \
		f = &p[i]; \
		f->kobj.kset = k; \
		ret = kobject_add(&f->kobj, NULL, "%d", i); \
		if (ret) { \
			netdev_warn(ndev->netdev, "Sysfs init error (%d)\n", \
				ret); \
			kobject_put(&f->kobj); \
			goto err; \
		} \
	} \
	return ret; \
err: \
	for (j = i - 1; j >= 0; --j) { \
		f = &p[j]; \
		kobject_del(&f->kobj); \
		kobject_put(&f->kobj); \
	} \
	kset_unregister(k); \
	return ret; \
} \
void kvx_kset_##s##_remove(struct kvx_eth_netdev *ndev, struct kset *k, \
			   struct kvx_eth_##s *p, size_t size) \
{ \
	struct kvx_eth_##s *f; \
	int i; \
	for (i = 0; i < size; ++i) { \
		f = &p[i]; \
		kobject_del(&f->kobj); \
		kobject_put(&f->kobj); \
	} \
	kset_unregister(k); \
}

kvx_declare_kset(lb_f, "lb")
kvx_declare_kset(rx_noc, "rx_noc")
kvx_declare_kset(tx_f, "tx")
kvx_declare_kset(cl_f, "pfc_cl")
kvx_declare_kset(dt_f, "dispatch_table")
kvx_declare_kset(parser_f, "parser")
kvx_declare_kset(rule_f, "rule")
kvx_declare_kset(phy_param, "param")
kvx_declare_kset(rx_bert_param, "rx_bert_param")
kvx_declare_kset(tx_bert_param, "tx_bert_param")

int kvx_eth_hw_sysfs_init(struct kvx_eth_hw *hw)
{
	int i, j, ret = 0;

	kobject_init(&hw->phy_f.kobj, &phy_f_ktype);
	kobject_init(&hw->lut_f.kobj, &lut_f_ktype);

	for (i = 0; i < KVX_ETH_LANE_NB; i++) {
		kobject_init(&hw->phy_f.param[i].kobj, &phy_param_ktype);
		kobject_init(&hw->phy_f.rx_ber[i].kobj, &rx_bert_param_ktype);
		kobject_init(&hw->phy_f.tx_ber[i].kobj, &tx_bert_param_ktype);
		kobject_init(&hw->lb_f[i].kobj, &lb_f_ktype);
		for (j = 0; j < NB_CLUSTER; j++)
			kobject_init(&hw->lb_f[i].rx_noc[j].kobj,
					&rx_noc_ktype);
	}

	for (i = 0; i < TX_FIFO_NB; i++)
		kobject_init(&hw->tx_f[i].kobj, &tx_f_ktype);

	for (i = 0; i < RX_DISPATCH_TABLE_ENTRY_ARRAY_SIZE; i++)
		kobject_init(&hw->dt_f[i].kobj, &dt_f_ktype);

	for (i = 0; i < KVX_ETH_PARSER_NB; i++) {
		kobject_init(&hw->parser_f[i].kobj, &parser_f_ktype);
		for (j = 0; j < KVX_NET_LAYER_NB; j++) {
			kobject_init(&hw->parser_f[i].rules[j].kobj,
					&rule_f_ktype);
		}
	}

	return ret;
}

int kvx_eth_netdev_sysfs_init(struct kvx_eth_netdev *ndev)
{
	struct kvx_eth_hw *hw = ndev->hw;
	int lane_id = ndev->cfg.id;
	int i, j, p, ret = 0;

	for (i = 0; i < KVX_ETH_PFC_CLASS_NB; i++)
		kobject_init(&ndev->cfg.cl_f[i].kobj, &cl_f_ktype);

	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		ret = kvx_eth_kobject_add(ndev->netdev, &ndev->cfg, &t[i]);
		if (ret)
			goto err;
	}

	ret = kobject_add(&hw->phy_f.kobj, &ndev->netdev->dev.kobj, "phy");
	if (ret)
		goto err;

	ret = kobject_add(&hw->lut_f.kobj, &ndev->netdev->dev.kobj, "lut");
	if (ret)
		goto err;

	ret = kvx_kset_phy_param_create(ndev, &hw->phy_f.kobj,
		phy_param_kset, &hw->phy_f.param[0], KVX_ETH_LANE_NB);
	if (ret)
		goto err;

	ret = kvx_kset_rx_bert_param_create(ndev, &hw->phy_f.kobj,
			 rx_bert_param_kset, &hw->phy_f.rx_ber[0],
			 KVX_ETH_LANE_NB);
	if (ret)
		goto err;

	ret = kvx_kset_tx_bert_param_create(ndev, &hw->phy_f.kobj,
			 tx_bert_param_kset, &hw->phy_f.tx_ber[0],
			 KVX_ETH_LANE_NB);
	if (ret)
		goto err;

	ret = kvx_kset_lb_f_create(ndev, &ndev->netdev->dev.kobj, lb_kset,
				   &hw->lb_f[lane_id], 1);
	if (ret)
		goto err;

	ret = kvx_kset_rx_noc_create(ndev, &ndev->hw->lb_f[lane_id].kobj,
				rx_noc_kset, &ndev->hw->lb_f[lane_id].rx_noc[0],
				NB_CLUSTER);
	if (ret)
		goto err;

	ret = kvx_kset_tx_f_create(ndev, &ndev->netdev->dev.kobj, tx_kset,
				   &hw->tx_f[0], TX_FIFO_NB);
	if (ret)
		goto err;

	ret = kvx_kset_cl_f_create(ndev, &ndev->netdev->dev.kobj, pfc_cl_kset,
				   &ndev->cfg.cl_f[0], KVX_ETH_PFC_CLASS_NB);
	if (ret)
		goto err;

	ret = kvx_kset_dt_f_create(ndev, &ndev->netdev->dev.kobj, dt_kset,
			&hw->dt_f[0], RX_DISPATCH_TABLE_ENTRY_ARRAY_SIZE);
	if (ret)
		goto err;

	ret = kvx_kset_parser_f_create(ndev, &ndev->netdev->dev.kobj,
			parser_kset, &hw->parser_f[0], KVX_ETH_PARSER_NB);
	if (ret)
		goto err;
	for (p = 0; p < KVX_ETH_PARSER_NB; p++) {
		ret = kvx_kset_rule_f_create(ndev, &ndev->hw->parser_f[p].kobj,
				rule_kset, &hw->parser_f[p].rules[0],
				KVX_NET_LAYER_NB);
		if (ret)
			goto err;
	}

	return ret;

err:
	for (j = i - 1; j >= 0; --j)
		kvx_eth_kobject_del(&ndev->cfg, &t[j]);

	kobject_del(&ndev->hw->lut_f.kobj);
	kobject_put(&ndev->hw->lut_f.kobj);
	kobject_del(&ndev->hw->phy_f.kobj);
	kobject_put(&ndev->hw->phy_f.kobj);
	return ret;
}

void kvx_eth_netdev_sysfs_uninit(struct kvx_eth_netdev *ndev)
{
	int i;

	kvx_kset_dt_f_remove(ndev, dt_kset, &ndev->hw->dt_f[0],
			RX_DISPATCH_TABLE_ENTRY_ARRAY_SIZE);
	kvx_kset_cl_f_remove(ndev, pfc_cl_kset, &ndev->cfg.cl_f[0],
			KVX_ETH_PFC_CLASS_NB);
	kvx_kset_tx_f_remove(ndev, tx_kset, &ndev->hw->tx_f[0], TX_FIFO_NB);
	for (i = 0; i < KVX_ETH_LANE_NB; i++) {
		kvx_kset_rx_noc_remove(ndev, rx_noc_kset,
				&ndev->hw->lb_f[i].rx_noc[0], NB_CLUSTER);
	}
	kvx_kset_lb_f_remove(ndev, lb_kset, &ndev->hw->lb_f[0],
			KVX_ETH_LANE_NB);
	kvx_kset_rx_bert_param_remove(ndev, rx_bert_param_kset,
			&ndev->hw->phy_f.rx_ber[0], KVX_ETH_LANE_NB);
	kvx_kset_tx_bert_param_remove(ndev, tx_bert_param_kset,
			&ndev->hw->phy_f.tx_ber[0], KVX_ETH_LANE_NB);
	kvx_kset_phy_param_remove(ndev, phy_param_kset,
			&ndev->hw->phy_f.param[0], KVX_ETH_LANE_NB);

	for (i = 0; i < KVX_ETH_PARSER_NB; i++)
		kvx_kset_rule_f_remove(ndev, rule_kset,
				&ndev->hw->parser_f[i].rules[0],
				KVX_NET_LAYER_NB);
	kvx_kset_parser_f_remove(ndev, parser_kset, &ndev->hw->parser_f[0],
			KVX_ETH_PARSER_NB);

	for (i = 0; i < ARRAY_SIZE(t); ++i)
		kvx_eth_kobject_del(&ndev->cfg, &t[i]);
	kobject_del(&ndev->hw->lut_f.kobj);
	kobject_put(&ndev->hw->lut_f.kobj);
	kobject_del(&ndev->hw->phy_f.kobj);
	kobject_put(&ndev->hw->phy_f.kobj);
}
