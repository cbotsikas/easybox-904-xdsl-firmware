#
# Copyright (C) 2006-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=yaddns
PKG_VERSION:=0.1.1
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
#PKG_SOURCE_URL:=@SF/yaddns
#PKG_MD5SUM:=2708d3e47a486b1872ef9c5537cd2f0d

PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/yaddns
  SUBMENU:=DDNS
  SECTION:=net
  CATEGORY:=Network
#  DEPENDS:=@BROKEN
  TITLE:=Yet Another DDNS client (DynDNS)
  URL:=http://patatrac.info/yaddns
endef

define Package/yaddns/description
Yaddns is a small ddns client with multiple ddns server supports (dyndns, ...)
and a high flexibility. Yaddns is particularly appropriate for embedded systems.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR)/
endef

define Package/yaddns/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/yaddns $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/etc/yaddns.init $(1)/etc/init.d/yaddns
#	$(INSTALL_DIR) $(1)/etc/config
#	$(INSTALL_DATA) $(PKG_BUILD_DIR)/etc/yaddns.conf $(1)/etc/config/yaddns.conf
endef

$(eval $(call BuildPackage,yaddns))
