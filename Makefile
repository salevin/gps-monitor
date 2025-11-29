include $(TOPDIR)/rules.mk

PKG_NAME:=gps-monitor
PKG_RELEASE:=1
PKG_VERSION:=1.0

include $(INCLUDE_DIR)/package.mk

define Package/gps-monitor
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=GPS Monitor for Omega2
	DEPENDS:=+libubus +libubox +libblobmsg-json +libncurses
endef

define Package/gps-monitor/description
	GPS monitoring tool for Omega2.
endef

define Build/Prepare
	$(Build/Prepare/Default)
endef

# Use the default compile rule (expects src/Makefile)
define Build/Compile
	$(call Build/Compile/Default)
endef

define Package/gps-monitor/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gps-monitor $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gps-logger $(1)/usr/bin/
endef

$(eval $(call BuildPackage,gps-monitor))
