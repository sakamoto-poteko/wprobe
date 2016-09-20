include $(TOPDIR)/rules.mk

PKG_NAME:=wprobed
PKG_VERSION:=0.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Wifi probe daemon
	DEPENDS:=+libpcap +libconfig +libsqlite3 +libstdcpp +libcurl +libpthread
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
	$(INSTALL_DIR) $(1)/etc/wprobed
	$(INSTALL_CONF) files/wprobed.cfg $(1)/etc/wprobed/wprobed.cfg
endef

$(eval $(call BuildPackage,wprobed))
