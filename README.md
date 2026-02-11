<p align="left">
<a href="https://github.com/ndeadly/spacedock-nx/blob/master/LICENSE"><img alt="GitHub" src="https://img.shields.io/github/license/ndeadly/spacedock-nx"></a>
<a href="https://github.com/ndeadly/spacedock-nx/releases"><img alt="GitHub All Releases" src="https://img.shields.io/github/downloads/ndeadly/spacedock-nx/total"></a>
<br>

# spacedock-nx

spacedock-nx is a Horizon based userland implementation of the Tegra TX1 bootrom exploit ([CVE-2018-6242](https://www.cve.org/CVERecord?id=CVE-2018-6242)) for Nintendo Switch. It can be used to inject an RCM payload from one Switch console to another, allowing a booted console running [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) to share payloads with a second console in RCM mode.

# Installation
- Download spacedock-nx.zip from the Releases page.
- Unzip the file to a location on your device.
- Transfer the unzipped contents to the root of your SD card, confirming any prompts to merge or overwrite if they appear.

# Usage
- Enter RCM mode on the target console if necessary.
- Launch spacedock-nx via hbmenu on the host console.
- Select a payload from the SD card. spacedock-nx checks the SD card root for `reboot_payload.bin`/`payload.bin` and the Hekate payloads directory (`sdmc:/bootloader/payloads`) for existing payloads. Additional payloads can be also be added to `sdmc:/config/spacedock-nx/payloads`. If no payloads can be found, an embedded version of fusee is used by default.
- Connect the host console to the RCM mode console using either a USB-C->USB-C cable or USB-C->USB-A cable + USB-C OTG adapter. If using an adapter, this must be connected to the host console. The payload will automatically be injected once the consoles are connected.
- You can disconnect the USB cable(s) once the payload is launched on the target.

# Credits
- __DavidBuchanan314__ for [fusee-nano](https://github.com/DavidBuchanan314/fusee-nano) from which this was heavily inspired.
- __reswitched__/__fail0verflow__ for the original Fusée Gelée and [ShofEL2](https://github.com/fail0verflow/shofel2) exploits.
- __switchbrew__ for the [libnx](https://github.com/switchbrew/libnx) library.
- __Atmosphere-NX__ for the fusee.bin payload and various libvapours utility macros borrowed from [Atmosphere-libs](https://github.com/Atmosphere-NX/Atmosphere-libs).