# LINUX_DIR is set by Buildroot, but not if running manually
ifeq ($(LINUX_DIR),)
LINUX_DIR := /lib/modules/$(shell uname -r)/build
endif

# BUILD_DIR is set by DKMS, but not if running manually
ifeq ($(BUILD_DIR),)
BUILD_DIR := .
endif

DTS_OUT_DIR := ./out

obj-m += beepy-kbd.o
beepy-kbd-objs += src/main.o src/params_iface.o src/sysfs_iface.o \
	src/input_iface.o src/input_meta.o src/input_display.o src/input_fw.o \
	src/input_rtc.o
ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

BOOT_ENV_FILE := /boot/armbianEnv.txt
BOOT_USER_OVERLAYS := user_overlays=beepy-kbd
KMAP_LINE := KMAP=/usr/share/kbd/keymaps/beepy-kbd.map

.PHONY: all clean install install_modules install_aux uninstall

all: $(DTS_OUT_DIR)/beepy-kbd.dts
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)'

$(DTS_OUT_DIR)/beepy-kbd.dts: ./beepy-kbd.dts $(DTS_OUT_DIR)
	cpp -nostdinc -I /usr/src/linux-headers-$(shell uname -r)/include -I include -I arch -undef -x assembler-with-cpp ./beepy-kbd.dts $@

$(DTS_OUT_DIR):
	mkdir $(DTS_OUT_DIR)

install_modules:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' modules_install
	# Rebuild dependencies
	depmod -A

install: install_modules install_aux

# Separate rule to be called from DKMS
install_aux: $(DTS_OUT_DIR)/beepy-kbd.dts
	# Compile and install device tree overlay
	# This will automatically place the compiled .dtbo in /boot/overlay-user
	# and append `user_overlays=beepy-kbd` to /boot/armbianEnv.txt.
	armbian-add-overlay $(DTS_OUT_DIR)/beepy-kbd.dts
	# Install keymap
	mkdir -p /usr/share/kbd/keymaps/
	install -D -m 0644 $(BUILD_DIR)/beepy-kbd.map /usr/share/kbd/keymaps/
	# Add auto-load module line if it wasn't already there
	grep -qxF 'beepy-kbd' /etc/modules \
		|| printf 'i2c-dev\nbeepy-kbd\n' >> /etc/modules
	# Configure keymap as default
	grep -qxF '$(KMAP_LINE)' /etc/default/keyboard \
		|| echo '$(KMAP_LINE)' >> /etc/default/keyboard
	rm -f /etc/console-setup/cached_setup_keyboard.sh
	dpkg-reconfigure keyboard-configuration \
		|| echo "dpkg-reconfigure failed, keymap may not be applied"

uninstall:
	# Remove auto-load module line and create a backup file
	sed -i.save '/beepy-kbd/d' /etc/modules
	# Remove user overlays and create a backup file
	# TODO: This will fail if there are multiple user overlays (like sharp-drm).
	#       We should just remove the beepy-kbd item in that case.
	sed -i.save '/$(BOOT_USER_OVERLAYS)/d' $(BOOT_ENV_FILE)
	# Remove device tree overlay
	rm -f /boot/overlay-user/beepy-kbd.dtbo
	# Remove keymap
	rm -f /usr/share/kbd/keymaps/beepy-kbd.map
	# Remove keymap setting
	sed -i.save '\|$(KMAP_LINE)|d' /etc/default/keyboard
	rm -f /etc/console-setup/cached_setup_keyboard.sh
	dpkg-reconfigure keyboard-configuration \
		|| echo "dpkg-reconfigure failed, old keymap may not be applied"

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(shell pwd)' clean
	rm -rf $(DTS_OUT_DIR)
