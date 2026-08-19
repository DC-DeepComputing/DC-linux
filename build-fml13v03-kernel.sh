#!/bin/bash
# Build DC-linux for DeepComputing FML13V03 on eic770x-v7.2-rc4.
#
# Follows eic7702-6.6.92/setenv.sh (ARCH=riscv, eic7702_defconfig),
# then turns on board-critical drivers that the FML13V03 DTS needs.
# Does not edit arch/riscv/configs/eic7702_defconfig.
#
# Usage:
#   ./build-fml13v03-kernel.sh            # Image + modules + dtbs
#   ./build-fml13v03-kernel.sh deb        # also bindeb-pkg
#   ./build-fml13v03-kernel.sh dtbs       # DTB only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

export ARCH=riscv
if command -v riscv64-unknown-linux-gnu-gcc >/dev/null 2>&1; then
	export CROSS_COMPILE=riscv64-unknown-linux-gnu-
elif command -v riscv64-linux-gnu-gcc >/dev/null 2>&1; then
	export CROSS_COMPILE=riscv64-linux-gnu-
else
	echo "No RISC-V cross gcc found (riscv64-unknown-linux-gnu- or riscv64-linux-gnu-)" >&2
	exit 1
fi

JOBS="${JOBS:-$(nproc)}"
LOCALVERSION="${LOCALVERSION:--eic7x-fml13v03}"
OUT="${OUT:-$ROOT/output/fml13v03}"
MODE="${1:-all}"

echo "ARCH=$ARCH"
echo "CROSS_COMPILE=$CROSS_COMPILE"
echo "CC=$(${CROSS_COMPILE}gcc --version | sed -n '1p')"
echo "JOBS=$JOBS"
echo "LOCALVERSION=$LOCALVERSION"
echo "MODE=$MODE"

kcfg() {
	# Enable a symbol as built-in. Unknown symbols are ignored after olddefconfig.
	./scripts/config --file .config --enable "$1" || true
}

kcfg_mod() {
	./scripts/config --file .config --module "$1" || true
}

enable_fml13v03_board_drivers() {
	echo "Enable FML13V03 board-critical drivers on top of eic7702_defconfig"

	# FML13V03 talks to Framework EC over SPI. CHROME_PLATFORMS now
	# includes RISCV so these options survive olddefconfig.
	kcfg CHROME_PLATFORMS
	kcfg CROS_EC
	kcfg CROS_EC_PROTO
	kcfg CROS_EC_SPI
	kcfg MFD
	kcfg MFD_CROS_EC_DEV
	kcfg CROS_EC_CHARDEV
	kcfg CROS_EC_SYSFS
	kcfg INPUT
	kcfg INPUT_KEYBOARD
	kcfg KEYBOARD_CROS_EC
	kcfg CROS_EC_POWEROFF
	kcfg CROS_USBPD_NOTIFY
	kcfg CHARGER_CROS_USBPD
	kcfg CROS_EC_TYPEC
	kcfg TYPEC

	# Board codec + ESWIN I2S (snps,i2s) + graph card
	kcfg SND
	kcfg SND_SOC
	kcfg SND_ESWIN_DW_I2S
	kcfg SND_SOC_ES8326
	kcfg SND_SIMPLE_CARD
	kcfg SND_AUDIO_GRAPH_CARD

	# LCD backlight + chassis fan (PWM described in board DTS)
	kcfg PWM
	kcfg PWM_ESWIN
	kcfg BACKLIGHT_CLASS_DEVICE
	kcfg BACKLIGHT_PWM
	kcfg HWMON
	kcfg SENSORS_PWM_FAN

	# Touchpad (hid-over-i2c @ 0x2c)
	kcfg HID
	kcfg HID_MULTITOUCH
	kcfg I2C_HID
	kcfg I2C_HID_OF

	# Ambient light (CM32181 @0x10 and CM32183 @0x29)
	kcfg IIO
	kcfg IIO_BUFFER
	kcfg CM32181

	# USB host (som560 already enables the four DWC3 ports)
	kcfg USB
	kcfg USB_SUPPORT
	kcfg USB_DWC3
	kcfg USB_DWC3_HOST
	kcfg USB_XHCI_HCD
	kcfg USB_XHCI_PLATFORM

	# Already in defconfig; force built-in so they are present at boot
	kcfg RFKILL
	kcfg RFKILL_GPIO
	kcfg REGULATOR
	kcfg REGULATOR_FIXED_VOLTAGE
	kcfg REGULATOR_TPS549D22
	kcfg SENSORS_TMP102
	kcfg EEPROM_AT24
	kcfg SPI
	kcfg SPI_MASTER
	kcfg SPI_DESIGNWARE
	kcfg SPI_DW_MMIO

	# AX210 on die1 PCIe (M.2 Key E)
	kcfg WLAN
	kcfg WLAN_VENDOR_INTEL
	kcfg_mod IWLWIFI
	kcfg_mod IWLMVM

	# Internal USB Laptop Camera (Realtek UVC 0bda:5634)
	kcfg MEDIA_SUPPORT
	kcfg MEDIA_CAMERA_SUPPORT
	kcfg MEDIA_USB_SUPPORT
	kcfg VIDEO_DEV
	kcfg_mod USB_VIDEO_CLASS
	kcfg I2C
	kcfg I2C_DESIGNWARE_CORE

	# FML13V03 DTB
	kcfg ARCH_ESWIN
	kcfg SOC_ESWIN_EIC7702
	kcfg OF
	kcfg BLK_DEV_INITRD

	make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" olddefconfig
}

build_dtbs() {
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" -j"$JOBS" dtbs
}

build_kernel() {
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" -j"$JOBS" \
		LOCALVERSION="$LOCALVERSION" \
		Image modules dtbs
}

install_outputs() {
	local dtb_dir=arch/riscv/boot/dts/eswin
	local fml_dtb=${dtb_dir}/eic7702-deepcomputing-fml13v03.dtb
	local d560_dtb=${dtb_dir}/eic7702-d560.dtb

	mkdir -p "$OUT"
	cp -av arch/riscv/boot/Image "$OUT/"
	cp -av System.map "$OUT/"
	cp -av .config "$OUT/config"
	cp -av "$fml_dtb" "$OUT/"
	if [ -f "$d560_dtb" ]; then
		cp -av "$d560_dtb" "$OUT/"
	fi
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" \
		LOCALVERSION="$LOCALVERSION" \
		INSTALL_MOD_PATH="$OUT" modules_install
	echo "Outputs in $OUT"
	ls -lh "$OUT/Image" "$OUT/eic7702-deepcomputing-fml13v03.dtb"
}

build_deb() {
	export KDEB_PKGVERSION="$(make -s kernelversion)-$(date +%Y.%m.%d.%H.%M)+"
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" -j"$JOBS" \
		LOCALVERSION="$LOCALVERSION" bindeb-pkg
	mkdir -p "$OUT"
	mv -v ../*.deb "$OUT/" 2>/dev/null || true
	ls -lh "$OUT"/*.deb 2>/dev/null || true
}

echo "=== defconfig ==="
make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" eic7702_defconfig
enable_fml13v03_board_drivers

echo "=== confirm board symbols ==="
for s in CROS_EC CROS_EC_SPI KEYBOARD_CROS_EC CROS_EC_POWEROFF \
	CHARGER_CROS_USBPD SND_SOC_ES8326 \
	SND_ESWIN_DW_I2S PWM_ESWIN BACKLIGHT_PWM SENSORS_PWM_FAN \
	I2C_HID_OF CM32181 HID_MULTITOUCH RFKILL_GPIO USB_DWC3; do
	grep -E "^CONFIG_${s}=" .config || echo "CONFIG_${s} not set"
done

case "$MODE" in
	dtbs)
		build_dtbs
		mkdir -p "$OUT"
		cp -av arch/riscv/boot/dts/eswin/eic7702-deepcomputing-fml13v03.dtb "$OUT/"
		;;
	deb)
		build_kernel
		install_outputs
		build_deb
		;;
	all|*)
		build_kernel
		install_outputs
		;;
esac

echo "DONE"
