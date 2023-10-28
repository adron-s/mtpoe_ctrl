#
# Copyright (C) 2018 adron@yapic.net
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mtpoe_ctrl
PKG_SOURCE_VERSION:=a3cbab7d8a7cf8aaace977b5ff8a307b9a10a3c5
PKG_MIRROR_HASH:=f7ea06ee8bf091011bfd36391747e8eedaea5e525da885c08757506456ae9774
PKG_VERSION:=1.15.1
PKG_RELEASE:=20

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/prudy/mtpoe_ctrl.git
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)

PKG_MAINTAINER:=Sergey Sergeev <adron@yapic.net>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/mtpoe_ctrl
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libuci +kmod-spi-dev
  TITLE:=Management utility for Mikrotiks PoE V2/V3/V4
  URL:=https://github.com/prudy/mtpoe_ctrl.git
endef

define Package/mtpoe_ctrl/description
	Management utility for Mikrotiks PoE V2/V3/V4
endef

define Package/mtpoe_ctrl/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bins/mtpoe_ctrl $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/openwrt/etc/init.d/mtpoe_ctrl $(1)/etc/init.d
endef

$(eval $(call BuildPackage,mtpoe_ctrl))
