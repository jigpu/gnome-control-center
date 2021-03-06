/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __NET_VPN_H
#define __NET_VPN_H

#include <glib-object.h>

#include "NetworkManagerVPN.h"
#include "net-object.h"
#include "nm-connection.h"
#include "nm-vpn-connection.h"

G_BEGIN_DECLS

#define NET_TYPE_VPN          (net_vpn_get_type ())
#define NET_VPN(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_VPN, NetVpn))
#define NET_VPN_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_VPN, NetVpnClass))
#define NET_IS_VPN(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_VPN))
#define NET_IS_VPN_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_VPN))
#define NET_VPN_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_VPN, NetVpnClass))

typedef struct _NetVpnPrivate         NetVpnPrivate;
typedef struct _NetVpn                NetVpn;
typedef struct _NetVpnClass           NetVpnClass;

struct _NetVpn
{
         NetObject               parent;
         NetVpnPrivate          *priv;
};

struct _NetVpnClass
{
        NetObjectClass               parent_class;
};

GType            net_vpn_get_type               (void);
NetVpn          *net_vpn_new                    (void);
void             net_vpn_set_connection         (NetVpn         *vpn,
                                                 NMConnection   *connection);
NMConnection    *net_vpn_get_connection         (NetVpn         *vpn);
const gchar     *net_vpn_get_service_type       (NetVpn         *vpn);
const gchar     *net_vpn_get_gateway            (NetVpn         *vpn);
const gchar     *net_vpn_get_id                 (NetVpn         *vpn);
const gchar     *net_vpn_get_username           (NetVpn         *vpn);
const gchar     *net_vpn_get_password           (NetVpn         *vpn);
NMVPNConnectionState net_vpn_get_state          (NetVpn         *vpn);

G_END_DECLS

#endif /* __NET_VPN_H */

