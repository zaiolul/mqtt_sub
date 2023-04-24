include $(TOPDIR)/rules.mk
 
PKG_NAME:=mqtt_sub
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/mqtt_sub
	CATEGORY:=Examples
	TITLE:=mqtt_sub
	DEPENDS:= +libmosquitto +libuci +libjson-c +libcurl +libsqlite3
endef

define Package/mqtt_sub/description
	mqtt subscriber
endef

define Package/mqtt_sub/install
	$(INSTALL_DIR) $(1)/usr/bin
	#$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mqtt_sub $(1)/usr/bin
	$(INSTALL_BIN) ./src/mqtt_messages.lua $(1)/usr/bin
	$(INSTALL_CONF) ./files/mqtt_sub.config $(1)/etc/config/mqtt_sub

endef

$(eval $(call BuildPackage,mqtt_sub))