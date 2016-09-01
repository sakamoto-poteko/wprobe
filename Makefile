#
# This software is licensed under the Public Domain.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=wprobed
PKG_VERSION:=0.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Wifi probe daemon
	DEPENDS:=+libconfig +libsqlite3 +libstdcpp +libcurl +libpthread +libpcap
endef

define Package/$(PKG_NAME)/description
	Wifi probe daemon
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wprobed $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) files/wprobed.init $(1)/etc/init.d/wprobed
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) files/wprobed.config $(1)/etc/config/wprobed
endef

$(eval $(call BuildPackage,wprobed))
