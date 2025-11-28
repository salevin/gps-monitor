# 📘 **README — Building Custom OpenWrt Packages for Onion Omega2 Using Docker**

This guide explains how to set up a **fully working package build environment** for the **Onion Omega2 / Omega2+ / Omega2 Pro**, using:

✅ Onion’s official OpenWrt 18.06 buildroot
✅ A Docker container as the build environment
✅ A custom package (`gps-monitor`)
✅ All troubleshooting steps (toolchain, menuconfig, etc.)

This README includes every fix needed to avoid the common “Nothing to be done for compile”, missing toolchain files, missing `libubus`, and menuconfig issues.

---

# 🐳 **1. Using the Docker SDK (recommended)**

Onion publishes a Docker image containing their complete build system.

Pull it:

```sh
docker pull onion/omega2-source:openwrt-18.06
```

Run the container, mounting your working directory:

```sh
docker run -it --rm -v $(pwd):/build onion/omega2-source:openwrt-18.06 /bin/bash
```

Inside the container:

* The full Onion OpenWrt source tree is located at:

  ```
  /root/source
  ```
* Your project files (mounted from the host) are in:

  ```
  /build
  ```

Navigate into the buildroot:

```sh
cd /root/source
```

---

# 📦 **2. Update Feeds**

```sh
./scripts/feeds update -a
./scripts/feeds install -a
```

This installs Onion packages and all required metadata.

---

# 🛠 **3. Build the Toolchain (Required Once)**

The Onion buildroot does NOT ship with a complete toolchain.
You must build it before compiling packages:

```sh
make toolchain/install -j$(nproc)
```

This can take several minutes depending on your CPU.
Once complete, you never need to rebuild unless you clean the toolchain.

---

# 📁 **4. Package Layout**

Place your package inside the buildroot:

```
/root/source/package/gps-monitor/
├── Makefile
└── src/
     └── gps-monitor.c
```

---

# 📝 **5. Package Makefile (OpenWrt-compatible)**

`package/gps-monitor/Makefile`:

```make
include $(TOPDIR)/rules.mk

PKG_NAME:=gps-monitor
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKGARCH:=all   # allow enabling the package in menuconfig

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/gps-monitor
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=GPS Monitor for Omega2
  DEPENDS:=+libubus +libubox +libblobmsg-json
endef

define Package/gps-monitor/description
  A GPS monitoring tool that displays GPS data from ubus.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-I$(STAGING_DIR)/usr/include \
		$(PKG_BUILD_DIR)/gps-monitor.c \
		-o $(PKG_BUILD_DIR)/gps-monitor \
		$(TARGET_LDFLAGS) -lubus -lubox -lblobmsg_json
endef

define Package/gps-monitor/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gps-monitor $(1)/usr/bin/
endef

$(eval $(call BuildPackage,gps-monitor))
```

Notes:

* **PKGARCH:=all** is required to allow enabling the package in `menuconfig`.
* Commands inside `define` blocks **must begin with a TAB**, not spaces.
* Section set to `Base system` ensures it's always selectable in menuconfig.

---

# 🧭 **6. Enable Your Package in menuconfig**

```sh
make menuconfig
```

Search for the package:

```
/
gps-monitor
```

Select it:

```
<M> gps-monitor
```

If it appears but **won’t toggle**, add:

```make
PKGARCH:=all
```

Then refresh metadata:

```sh
rm -rf tmp/.packageinfo
make package/symlinks
make menuconfig
```

If still stuck, manually force-enable it by editing `.config`:

```sh
echo "CONFIG_PACKAGE_gps-monitor=m" >> .config
make oldconfig
```

---

# 🔨 **7. Build the Package**

```sh
make package/gps-monitor/{clean,compile} V=s -j$(nproc)
```

You should see:

* Build/Prepare copying source files
* Build/Compile invoking mipsel-openwrt-linux-musl-gcc
* The binary produced in `build_dir`
* The package assembled into `.ipk`

---

# 📦 **8. Output Location**

Your `.ipk` appears here:

```
bin/packages/mipsel_24kc/base/gps-monitor_1.0-1_mipsel_24kc.ipk
```

or:

```
bin/targets/ramips/mt76x8/packages/
```

---

# 📥 **9. Deploying to the Omega2**

Copy to device:

```sh
scp bin/packages/mipsel_24kc/base/gps-monitor_*.ipk root@omega2:/root/
```

Install:

```sh
opkg update
opkg install /root/gps-monitor_*.ipk
```

---

# 🧩 **Troubleshooting**

### **Problem: “Nothing to be done for 'compile'”**

Causes:

* Package not enabled in `.config`
* Build/Prepare skipped (tabs missing)
* PKG_BUILD_DIR empty
* menuconfig blocking selection
* stale metadata

Fix:

```sh
echo "CONFIG_PACKAGE_gps-monitor=m" >> .config
make oldconfig
```

---

### **Problem: Can see package in menuconfig but cannot select**

Fix:

```make
PKGARCH:=all
```

Refresh:

```sh
rm -rf tmp/.packageinfo
make package/symlinks
make menuconfig
```

---

### **Problem: Missing libubus, libubox, blobmsg-json**

Build them:

```sh
make package/system/ubus/compile -j$(nproc)
make package/system/ubox/compile -j$(nproc)
make package/libs/libubox/compile -j$(nproc)
make package/libs/libjson-c/compile -j$(nproc)
```

---

# 🎉 **Done!**

You now have a fully functional, Dockerized, Onion Omega2-compatible OpenWrt build environment, and a clean working package (`gps-monitor`) that builds reproducibly.

