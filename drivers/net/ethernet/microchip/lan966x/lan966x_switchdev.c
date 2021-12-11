// SPDX-License-Identifier: GPL-2.0+

#include <linux/if_bridge.h>
#include <net/switchdev.h>

#include "lan966x_main.h"

static struct notifier_block lan966x_netdevice_nb __read_mostly;
static struct notifier_block lan966x_switchdev_nb __read_mostly;
static struct notifier_block lan966x_switchdev_blocking_nb __read_mostly;

static LIST_HEAD(ext_entries);

struct lan966x_ext_entry {
	struct list_head list;
	struct net_device *dev;
	u32 ports;
	struct lan966x *lan966x;
};

static void lan966x_port_bridge_flags(struct lan966x_port *port,
				      struct switchdev_brport_flags flags)
{
	u32 val = lan_rd(port->lan966x, ANA_PGID(PGID_MC));

	val = ANA_PGID_PGID_GET(val);

	if (flags.mask & BR_MCAST_FLOOD) {
		if (flags.val & BR_MCAST_FLOOD)
			val |= BIT(port->chip_port);
		else
			val &= ~BIT(port->chip_port);
	}

	lan_rmw(ANA_PGID_PGID_SET(val),
		ANA_PGID_PGID,
		port->lan966x, ANA_PGID(PGID_MC));
}

static int lan966x_port_pre_bridge_flags(struct lan966x_port *port,
					 struct switchdev_brport_flags flags)
{
	if (flags.mask & ~BR_MCAST_FLOOD)
		return -EINVAL;

	return 0;
}

static void lan966x_update_fwd_mask(struct lan966x *lan966x)
{
	int i;

	for (i = 0; i < lan966x->num_phys_ports; i++) {
		struct lan966x_port *port = lan966x->ports[i];
		unsigned long mask = 0;

		if (port && lan966x->bridge_fwd_mask & BIT(i))
			mask = lan966x->bridge_fwd_mask & ~BIT(i);

		mask |= BIT(CPU_PORT);

		lan_wr(ANA_PGID_PGID_SET(mask),
		       lan966x, ANA_PGID(PGID_SRC + i));
	}
}

static void lan966x_port_stp_state_set(struct lan966x_port *port, u8 state)
{
	struct lan966x *lan966x = port->lan966x;
	bool learn_ena = false;

	if (state == BR_STATE_FORWARDING || state == BR_STATE_LEARNING)
		learn_ena = true;

	if (state == BR_STATE_FORWARDING)
		lan966x->bridge_fwd_mask |= BIT(port->chip_port);
	else
		lan966x->bridge_fwd_mask &= ~BIT(port->chip_port);

	lan_rmw(ANA_PORT_CFG_LEARN_ENA_SET(learn_ena),
		ANA_PORT_CFG_LEARN_ENA,
		lan966x, ANA_PORT_CFG(port->chip_port));

	lan966x_update_fwd_mask(lan966x);
}

static void lan966x_port_ageing_set(struct lan966x_port *port,
				    unsigned long ageing_clock_t)
{
	unsigned long ageing_jiffies = clock_t_to_jiffies(ageing_clock_t);
	u32 ageing_time = jiffies_to_msecs(ageing_jiffies) / 1000;

	lan966x_mac_set_ageing(port->lan966x, ageing_time);
}

static int lan966x_port_attr_set(struct net_device *dev, const void *ctx,
				 const struct switchdev_attr *attr,
				 struct netlink_ext_ack *extack)
{
	struct lan966x_port *port = netdev_priv(dev);
	int err = 0;

	if (ctx && ctx != port)
		return 0;

	switch (attr->id) {
	case SWITCHDEV_ATTR_ID_PORT_BRIDGE_FLAGS:
		lan966x_port_bridge_flags(port, attr->u.brport_flags);
		break;
	case SWITCHDEV_ATTR_ID_PORT_PRE_BRIDGE_FLAGS:
		err = lan966x_port_pre_bridge_flags(port, attr->u.brport_flags);
		break;
	case SWITCHDEV_ATTR_ID_PORT_STP_STATE:
		lan966x_port_stp_state_set(port, attr->u.stp_state);
		break;
	case SWITCHDEV_ATTR_ID_BRIDGE_AGEING_TIME:
		lan966x_port_ageing_set(port, attr->u.ageing_time);
		break;
	case SWITCHDEV_ATTR_ID_BRIDGE_VLAN_FILTERING:
		lan966x_vlan_port_set_vlan_aware(port, attr->u.vlan_filtering);
		lan966x_vlan_port_apply(port);
		lan966x_vlan_cpu_set_vlan_aware(port);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}

static int lan966x_port_bridge_join(struct lan966x_port *port,
				    struct net_device *bridge,
				    struct netlink_ext_ack *extack)
{
	struct lan966x *lan966x = port->lan966x;
	struct net_device *dev = port->dev;
	int err;

	if (!lan966x->bridge_mask) {
		lan966x->bridge = bridge;
	} else {
		if (lan966x->bridge != bridge)
			return -ENODEV;
	}

	err = switchdev_bridge_port_offload(dev, dev, port,
					    &lan966x_switchdev_nb,
					    &lan966x_switchdev_blocking_nb,
					    false, extack);
	if (err)
		return err;

	lan966x->bridge_mask |= BIT(port->chip_port);

	return 0;
}

static void lan966x_port_bridge_leave(struct lan966x_port *port,
				      struct net_device *bridge)
{
	struct lan966x *lan966x = port->lan966x;

	lan966x->bridge_mask &= ~BIT(port->chip_port);

	if (!lan966x->bridge_mask)
		lan966x->bridge = NULL;

	/* Set the port back to host mode */
	lan966x_vlan_port_set_vlan_aware(port, false);
	lan966x_vlan_port_set_vid(port, HOST_PVID, false, false);
	lan966x_vlan_port_apply(port);

	lan966x_mac_cpu_learn(lan966x, port->dev->dev_addr, HOST_PVID);
}

static int lan966x_port_changeupper(struct net_device *dev,
				    struct netdev_notifier_changeupper_info *info)
{
	struct lan966x_port *port = netdev_priv(dev);
	struct netlink_ext_ack *extack;
	int err = 0;

	extack = netdev_notifier_info_to_extack(&info->info);

	if (netif_is_bridge_master(info->upper_dev)) {
		if (info->linking)
			err = lan966x_port_bridge_join(port, info->upper_dev,
						       extack);
		else
			lan966x_port_bridge_leave(port, info->upper_dev);
	}

	return err;
}

static int lan966x_port_prechangeupper(struct net_device *dev,
				       struct netdev_notifier_changeupper_info *info)
{
	struct lan966x_port *port = netdev_priv(dev);

	if (netif_is_bridge_master(info->upper_dev) && !info->linking)
		switchdev_bridge_port_unoffload(port->dev, port,
						&lan966x_switchdev_nb,
						&lan966x_switchdev_blocking_nb);

	return NOTIFY_DONE;
}

static int lan966x_port_add_addr(struct net_device *dev, bool up)
{
	struct lan966x_port *port = netdev_priv(dev);
	struct lan966x *lan966x = port->lan966x;
	u16 vid;

	vid = lan966x_vlan_port_get_pvid(port);

	if (up)
		lan966x_mac_cpu_learn(lan966x, dev->dev_addr, vid);
	else
		lan966x_mac_cpu_forget(lan966x, dev->dev_addr, vid);

	return 0;
}

static struct lan966x_ext_entry *lan966x_ext_find_entry(struct net_device *dev)
{
	struct lan966x_ext_entry *ext_entry;

	list_for_each_entry(ext_entry, &ext_entries, list) {
		if (ext_entry->dev == dev)
			return ext_entry;
	}

	return NULL;
}

static bool lan966x_ext_add_entry(struct net_device *dev, void *lan966x)
{
	struct lan966x_ext_entry *ext_entry;

	ext_entry = lan966x_ext_find_entry(dev);
	if (ext_entry) {
		if (ext_entry->lan966x)
			return false;

		ext_entry->ports++;
		return true;
	}

	ext_entry = kzalloc(sizeof(*ext_entry), GFP_KERNEL);
	if (!ext_entry)
		return false;

	ext_entry->dev = dev;
	ext_entry->ports = 1;
	ext_entry->lan966x = lan966x;
	list_add_tail(&ext_entry->list, &ext_entries);
	return true;
}

static void lan966x_ext_remove_entry(struct net_device *dev)
{
	struct lan966x_ext_entry *ext_entry;

	ext_entry = lan966x_ext_find_entry(dev);
	if (!ext_entry)
		return;

	ext_entry->ports--;
	if (!ext_entry->ports) {
		list_del(&ext_entry->list);
		kfree(ext_entry);
	}
}

void lan966x_ext_purge_entries(void)
{
	struct lan966x_ext_entry *ext_entry, *tmp;

	list_for_each_entry_safe(ext_entry, tmp, &ext_entries, list) {
		list_del(&ext_entry->list);
		kfree(ext_entry);
	}
}

static int lan966x_ext_check_entry(struct net_device *dev,
				   unsigned long event,
				   void *ptr)
{
	struct netdev_notifier_changeupper_info *info;

	if (event != NETDEV_PRECHANGEUPPER)
		return 0;

	info = ptr;
	if (!netif_is_bridge_master(info->upper_dev))
		return 0;

	if (info->linking) {
		if (!lan966x_ext_add_entry(info->upper_dev, NULL))
			return -EOPNOTSUPP;
	} else {
		lan966x_ext_remove_entry(info->upper_dev);
	}

	return NOTIFY_DONE;
}

static bool lan966x_port_ext_check_entry(struct net_device *dev,
					 struct netdev_notifier_changeupper_info *info)
{
	struct lan966x_port *port = netdev_priv(dev);
	struct lan966x *lan966x = port->lan966x;
	struct lan966x_ext_entry *entry;

	if (!netif_is_bridge_master(info->upper_dev))
		return true;

	entry = lan966x_ext_find_entry(info->upper_dev);
	if (info->linking) {
		if (!entry)
			return lan966x_ext_add_entry(info->upper_dev, lan966x);

		if (entry->lan966x == lan966x) {
			entry->ports++;
			return true;
		}
	} else {
		lan966x_ext_remove_entry(info->upper_dev);
		return true;
	}

	return false;
}

static int lan966x_netdevice_port_event(struct net_device *dev,
					struct notifier_block *nb,
					unsigned long event, void *ptr)
{
	int err = 0;

	if (!lan966x_netdevice_check(dev))
		return lan966x_ext_check_entry(dev, event, ptr);

	switch (event) {
	case NETDEV_PRECHANGEUPPER:
		if (!lan966x_port_ext_check_entry(dev, ptr))
			return -EOPNOTSUPP;

		err = lan966x_port_prechangeupper(dev, ptr);
		break;
	case NETDEV_CHANGEUPPER:
		err = lan966x_port_changeupper(dev, ptr);
		break;
	case NETDEV_PRE_UP:
		err = lan966x_port_add_addr(dev, true);
		break;
	case NETDEV_DOWN:
		err = lan966x_port_add_addr(dev, false);
		break;
	}

	return err;
}

static int lan966x_netdevice_event(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	int ret;

	ret = lan966x_netdevice_port_event(dev, nb, event, ptr);

	return notifier_from_errno(ret);
}

static int lan966x_switchdev_event(struct notifier_block *nb,
				   unsigned long event, void *ptr)
{
	struct net_device *dev = switchdev_notifier_info_to_dev(ptr);
	int err;

	switch (event) {
	case SWITCHDEV_PORT_ATTR_SET:
		err = switchdev_handle_port_attr_set(dev, ptr,
						     lan966x_netdevice_check,
						     lan966x_port_attr_set);
		return notifier_from_errno(err);
	}

	return NOTIFY_DONE;
}

static int lan966x_handle_port_vlan_add(struct lan966x_port *port,
					const struct switchdev_obj *obj)
{
	const struct switchdev_obj_port_vlan *v = SWITCHDEV_OBJ_PORT_VLAN(obj);
	struct lan966x *lan966x = port->lan966x;

	/* When adding a port to a vlan, we get a callback for the port but
	 * also for the bridge. When get the callback for the bridge just bail
	 * out. Then when the bridge is added to the vlan, then we get a
	 * callback here but in this case the flags has set:
	 * BRIDGE_VLAN_INFO_BRENTRY. In this case it means that the CPU
	 * port is added to the vlan, so the broadcast frames and unicast frames
	 * with dmac of the bridge should be foward to CPU.
	 */
	if (netif_is_bridge_master(obj->orig_dev) &&
	    !(v->flags & BRIDGE_VLAN_INFO_BRENTRY))
		return 0;

	if (!netif_is_bridge_master(obj->orig_dev))
		return lan966x_vlan_port_add_vlan(port, v->vid,
						  v->flags & BRIDGE_VLAN_INFO_PVID,
						  v->flags & BRIDGE_VLAN_INFO_UNTAGGED);

	if (netif_is_bridge_master(obj->orig_dev))
		return lan966x_vlan_cpu_add_vlan(lan966x, obj->orig_dev, v->vid);

	return 0;
}

static int lan966x_handle_port_obj_add(struct net_device *dev, const void *ctx,
				       const struct switchdev_obj *obj,
				       struct netlink_ext_ack *extack)
{
	struct lan966x_port *port = netdev_priv(dev);
	int err;

	if (ctx && ctx != port)
		return 0;

	switch (obj->id) {
	case SWITCHDEV_OBJ_ID_PORT_VLAN:
		err = lan966x_handle_port_vlan_add(port, obj);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}

static int lan966x_handle_port_vlan_del(struct lan966x_port *port,
					const struct switchdev_obj *obj)
{
	const struct switchdev_obj_port_vlan *v = SWITCHDEV_OBJ_PORT_VLAN(obj);
	struct lan966x *lan966x = port->lan966x;

	/* In case the physical port gets called */
	if (!netif_is_bridge_master(obj->orig_dev))
		return lan966x_vlan_port_del_vlan(port, v->vid);

	/* In case the bridge gets called */
	if (netif_is_bridge_master(obj->orig_dev))
		return lan966x_vlan_cpu_del_vlan(lan966x, obj->orig_dev, v->vid);

	return 0;
}

static int lan966x_handle_port_obj_del(struct net_device *dev, const void *ctx,
				       const struct switchdev_obj *obj)
{
	struct lan966x_port *port = netdev_priv(dev);
	int err;

	if (ctx && ctx != port)
		return 0;

	switch (obj->id) {
	case SWITCHDEV_OBJ_ID_PORT_VLAN:
		err = lan966x_handle_port_vlan_del(port, obj);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}

static int lan966x_switchdev_blocking_event(struct notifier_block *nb,
					    unsigned long event,
					    void *ptr)
{
	struct net_device *dev = switchdev_notifier_info_to_dev(ptr);
	int err;

	switch (event) {
	case SWITCHDEV_PORT_OBJ_ADD:
		err = switchdev_handle_port_obj_add(dev, ptr,
						    lan966x_netdevice_check,
						    lan966x_handle_port_obj_add);
		return notifier_from_errno(err);
	case SWITCHDEV_PORT_OBJ_DEL:
		err = switchdev_handle_port_obj_del(dev, ptr,
						    lan966x_netdevice_check,
						    lan966x_handle_port_obj_del);
		return notifier_from_errno(err);
	case SWITCHDEV_PORT_ATTR_SET:
		err = switchdev_handle_port_attr_set(dev, ptr,
						     lan966x_netdevice_check,
						     lan966x_port_attr_set);
		return notifier_from_errno(err);
	}

	return NOTIFY_DONE;
}

static struct notifier_block lan966x_netdevice_nb __read_mostly = {
	.notifier_call = lan966x_netdevice_event,
};

static struct notifier_block lan966x_switchdev_nb __read_mostly = {
	.notifier_call = lan966x_switchdev_event,
};

static struct notifier_block lan966x_switchdev_blocking_nb __read_mostly = {
	.notifier_call = lan966x_switchdev_blocking_event,
};

void lan966x_register_notifier_blocks(struct lan966x *lan966x)
{
	register_netdevice_notifier(&lan966x_netdevice_nb);
	register_switchdev_notifier(&lan966x_switchdev_nb);
	register_switchdev_blocking_notifier(&lan966x_switchdev_blocking_nb);
}

void lan966x_unregister_notifier_blocks(struct lan966x *lan966x)
{
	unregister_switchdev_blocking_notifier(&lan966x_switchdev_blocking_nb);
	unregister_switchdev_notifier(&lan966x_switchdev_nb);
	unregister_netdevice_notifier(&lan966x_netdevice_nb);
}
