/* Kernel module to match the port-ranges, trigger related port-ranges,
  * and alters the destination to a local IP address.
  *
  * Copyright (C) 2003, CyberTAN Corporation
  * All Rights Reserved.
  *
  * Description:
  *   This is kernel module for port-triggering.
  *
  *   The module follows the Netfilter framework, called extended packet
  *   matching modules.
  */

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <net/protocol.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4.h>
#ifdef CONFIG_NF_NAT_NEEDED
#include <net/netfilter/nf_nat_rule.h>
#else
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#endif
#include <linux/netfilter_ipv4/ipt_TRIGGER.h>

DEFINE_RWLOCK(ip_conntrack_lock);

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif


struct ipt_trigger {
        struct list_head list;          /* Trigger list */
        struct timer_list timeout;      /* Timer for list destroying */
        u_int32_t srcip;                /* Outgoing source address */
        u_int32_t dstip;                /* Outgoing destination address */
        u_int16_t mproto;               /* Trigger protocol */
        u_int16_t rproto;               /* Related protocol */
        struct ipt_trigger_ports ports; /* Trigger and related ports */
       u_int8_t reply;                 /* Confirm a reply connection */
};

LIST_HEAD(trigger_list);
//DECLARE_LOCK(ip_trigger_lock);

static void trigger_refresh(struct ipt_trigger *trig, unsigned long extra_jiffies)
{
   DEBUGP("%s: \n", __FUNCTION__);
    NF_CT_ASSERT(trig);
	  write_lock_bh(&ip_conntrack_lock);
    /* Need del_timer for race avoidance (may already be dying). */
     if (del_timer(&trig->timeout)) {
        trig->timeout.expires = jiffies + extra_jiffies;
        add_timer(&trig->timeout);
    }

		write_unlock_bh(&ip_conntrack_lock);
}

static void __del_trigger(struct ipt_trigger *trig)
{
    DEBUGP("%s: \n", __FUNCTION__);
    NF_CT_ASSERT(trig);

     /* delete from 'trigger_list' */
    list_del(&trig->list);
    kfree(trig);
}

static void trigger_timeout(unsigned long ul_trig)
{
    struct ipt_trigger *trig= (void *) ul_trig;

    DEBUGP("trigger list %p timed out\n", trig);
		write_lock_bh(&ip_conntrack_lock);
    __del_trigger(trig);
	  write_unlock_bh(&ip_conntrack_lock);
}

static unsigned int
add_new_trigger(struct ipt_trigger *trig)
{
    struct ipt_trigger *new;

    DEBUGP("!!!!!!!!!!!! %s !!!!!!!!!!!\n", __FUNCTION__);
	  write_lock_bh(&ip_conntrack_lock);
    new = (struct ipt_trigger *)kmalloc(sizeof(struct ipt_trigger), GFP_ATOMIC);

    if (!new) {
		  write_unlock_bh(&ip_conntrack_lock);
        DEBUGP("%s: OOM allocating trigger list\n", __FUNCTION__);
        return -ENOMEM;
    }
    memset(new, 0, sizeof(*trig));
    INIT_LIST_HEAD(&new->list);
    memcpy(new, trig, sizeof(*trig));

    /* add to global table of trigger */
	list_add (&new->list, &trigger_list);
    /* add and start timer if required */
    init_timer(&new->timeout);
    new->timeout.data = (unsigned long)new;
    new->timeout.function = trigger_timeout;
    new->timeout.expires = jiffies + (TRIGGER_TIMEOUT * HZ);
    add_timer(&new->timeout);

 	write_unlock_bh(&ip_conntrack_lock);
	return 0;
}

static inline int trigger_out_matched(const struct ipt_trigger *i,
        const u_int16_t proto, const u_int16_t dport)
{
    /* DEBUGP("%s: i=%p, proto= %d, dport=%d.\n", __FUNCTION__, i, proto, dport);
    DEBUGP("%s: Got one, mproto= %d, mport[0..1]=%d, %d.\n", __FUNCTION__,
            i->mproto, i->ports.mport[0], i->ports.mport[1]); */

    return ((i->mproto == proto) && (i->ports.mport[0] <= dport)
            && (i->ports.mport[1] >= dport));
}

static unsigned int
trigger_out(struct sk_buff **pskb, const struct xt_target_param *par)
{
    const struct ipt_trigger_info *info = par->targinfo;
    struct ipt_trigger trig, *found;
    const struct iphdr *iph = ip_hdr(*pskb); //(*pskb)->nh.iph;
    struct tcphdr *tcph = (void *)iph + iph->ihl*4;     /* Might be TCP, UDP */

    DEBUGP("############# %s ############\n", __FUNCTION__);
    /* Check if the trigger range has already existed in 'trigger_list'. */
	list_for_each_entry(found, &trigger_list, list){
		if (trigger_out_matched(found, iph->protocol,ntohs(tcph->dest))) {
        /* Yeah, it exists. We need to update(delay) the destroying timer. */
        trigger_refresh(found, TRIGGER_TIMEOUT * HZ);
        /* In order to allow multiple hosts use the same port range, we update
           the 'saddr' after previous trigger has a reply connection. */
        if (found->reply)
            found->srcip = iph->saddr;
		return IPT_CONTINUE;        /* We don't block any packet. */
		}
	}
	DEBUGP("############# %s ############  not found the entry\n", __FUNCTION__);
	DEBUGP("src ip:%x match protocol:%d(%d-%d) related proto:%d(%d-%d)\n"
			, iph->saddr
			, iph->protocol, info->ports.mport[0], info->ports.mport[1]
			, info->proto, info->ports.rport[0], info->ports.rport[1]);
		/* Create new trigger */
		memset(&trig, 0, sizeof(trig));
		trig.srcip = iph->saddr;
		trig.mproto = iph->protocol;
		trig.rproto = info->proto;
		memcpy(&trig.ports, &info->ports, sizeof(struct ipt_trigger_ports));
		add_new_trigger(&trig); /* Add the new 'trig' to list 'trigger_list'. */
	return IPT_CONTINUE;        /* We don't block any packet. */
}

static inline int trigger_in_matched(const struct ipt_trigger *i,
        const u_int16_t proto, const u_int16_t dport)
{
    /* DEBUGP("%s: i=%p, proto= %d, dport=%d.\n", __FUNCTION__, i, proto, dport);
    DEBUGP("%s: Got one, rproto= %d, rport[0..1]=%d, %d.\n", __FUNCTION__,
           i->rproto, i->ports.rport[0], i->ports.rport[1]); */
    u_int16_t rproto = i->rproto;

    if (!rproto)
        rproto = proto;

    return ((rproto == proto) && (i->ports.rport[0] <= dport)
            && (i->ports.rport[1] >= dport));
}

static unsigned int
trigger_in(struct sk_buff **pskb, const struct xt_target_param *par)
{
    struct ipt_trigger *found;
    const struct iphdr *iph = ip_hdr(*pskb); //(*pskb)->nh.iph;
    struct tcphdr *tcph = (void *)iph + iph->ihl*4;     /* Might be TCP, UDP */
    /* Check if the trigger-ed range has already existed in 'trigger_list'. */
	list_for_each_entry(found, &trigger_list, list){
	        if (trigger_in_matched(found, iph->protocol,ntohs(tcph->dest))) {
        /* Yeah, it exists. We need to update(delay) the destroying timer. */
        trigger_refresh(found, TRIGGER_TIMEOUT * HZ);
        return NF_ACCEPT;       /* Accept it, or the imcoming packet could be
                                   dropped in the FORWARD chain */
		}
	}
    return IPT_CONTINUE;        /* Our job is the interception. */
}

static unsigned int
trigger_dnat(struct sk_buff **pskb, const struct xt_target_param *par)
{
    struct ipt_trigger *found;
    const struct iphdr *iph = ip_hdr(*pskb); //(*pskb)->nh.iph;
    struct tcphdr *tcph = (void *)iph + iph->ihl*4;     /* Might be TCP, UDP */
    struct nf_conn *ct;
    enum ip_conntrack_info ctinfo;
    const struct nf_nat_multi_range_compat *mr=par->targinfo;
	struct nf_nat_range newrange;

    NF_CT_ASSERT(par->hooknum == NF_INET_PRE_ROUTING);
    /* Check if the trigger-ed range has already existed in 'trigger_list'. */
	list_for_each_entry(found, &trigger_list, list){
		if (trigger_in_matched(found, iph->protocol,ntohs(tcph->dest))) {
			if (!found->srcip)
				return IPT_CONTINUE;
    found->reply = 1;   /* Confirm there has been a reply connection. */
    ct = nf_ct_get(*pskb, &ctinfo);
    NF_CT_ASSERT(ct && (ctinfo == IP_CT_NEW));

    DEBUGP("%s: got %x ", __FUNCTION__, found->srcip);

	/* Alter the destination of incoming packet. */
 	newrange= ((struct nf_nat_range)
		{ mr->range[0].flags | IP_NAT_RANGE_MAP_IPS,
		  found->srcip, found->srcip,
		  mr->range[0].min, mr->range[0].max });

	/* Hand modified range to generic setup. */
	return nf_nat_setup_info(ct, &newrange, HOOK2MANIP(par->hooknum));
		}
	}

	return IPT_CONTINUE;    /* We don't block any packet. */
}

static unsigned int
target_trigger(struct sk_buff *skb, const struct xt_target_param *par)
{

    const struct ipt_trigger_info *info = par->targinfo;
    const struct iphdr *iph = ip_hdr(skb);  //(*pskb)->nh.iph;
    struct sk_buff *pskb;
	pskb = (struct sk_buff *)skb;

DEBUGP("%s: type = %s\n", __FUNCTION__,
            (info->type == IPT_TRIGGER_DNAT) ? "dnat" :
            (info->type == IPT_TRIGGER_IN) ? "in" : "out");

    /* The Port-trigger only supports TCP and UDP. */
    if ((iph->protocol != IPPROTO_TCP) && (iph->protocol != IPPROTO_UDP))
        return IPT_CONTINUE;

    if (info->type == IPT_TRIGGER_OUT)
        return trigger_out(&pskb, par);
    else if (info->type == IPT_TRIGGER_IN)
        return trigger_in(&pskb, par);
    else if (info->type == IPT_TRIGGER_DNAT)
        return trigger_dnat(&pskb, par);
    return IPT_CONTINUE;

}

static bool
checkentry_trigger(const struct xt_tgchk_param *par)
{
        const struct ipt_trigger_info *info = par->targinfo;
        struct list_head *cur_item, *tmp_item;

         if ((strcmp(par->table, "mangle") == 0)) {
                DEBUGP("trigger_check: bad table `%s'.\n", par->table);
                return 0;
        }
        if (par->hook_mask & ~((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_FORWARD))) {
                DEBUGP("trigger_check: bad hooks %x.\n", par->hook_mask);
                return 0;
        }
        if (info->proto) {
            if (info->proto != IPPROTO_TCP && info->proto != IPPROTO_UDP) {
                DEBUGP("trigger_check: bad proto %d.\n", info->proto);
                return 0;
            }
        }
        if (info->type == IPT_TRIGGER_OUT) {
            if (!info->ports.mport[0] || !info->ports.rport[0]) {
                DEBUGP("trigger_check: Try 'iptbles -j TRIGGER -h' for help.\n");
               return 0;
            }
        }

        /* Empty the 'trigger_list' */
        list_for_each_safe(cur_item, tmp_item, &trigger_list) {
            struct ipt_trigger *trig = (void *)cur_item;

            DEBUGP("%s: list_for_each_safe(): %p.\n", __FUNCTION__, trig);
            del_timer(&trig->timeout);
            __del_trigger(trig);
        }

        return 1;
}


static struct xt_target ipt_trigger_reg = {
        .name           = "TRIGGER",
	.family         = NFPROTO_IPV4,
	.target         = target_trigger,
        .targetsize     = sizeof(struct ipt_trigger_info),
        .checkentry     = checkentry_trigger,
        .me             = THIS_MODULE,
};

static int __init init(void)
{
	printk("iptables target:TRIGGER __init\n");
        return xt_register_target(&ipt_trigger_reg);
}

static void __exit fini(void)
{
         xt_unregister_target(&ipt_trigger_reg);
}

module_init(init);
module_exit(fini);
