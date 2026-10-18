#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "scssdk_telemetry.h"
#include "eurotrucks2/scssdk_eut2.h"
#include "eurotrucks2/scssdk_telemetry_eut2.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_truck_common_channels.h"

namespace {

constexpr USHORT kLogitechVendorId = 0x046d;
constexpr USHORT kG29ProductId = 0xc24f;
constexpr float kDefaultRpmLimit = 2500.0f;
constexpr ULONGLONG kAnimationStepMs = 100;
constexpr unsigned char kStartupSweep[] = {
    0x01, 0x03, 0x07, 0x0f, 0x1f, // Green side to red side.
    0x0f, 0x07, 0x03, 0x01, 0x00  // Red side back to green, then off.
};
constexpr unsigned kStartupSweepCount = 3;

HANDLE g_wheel = INVALID_HANDLE_VALUE;
USHORT g_output_report_length = 0;
scs_log_t g_log = nullptr;
float g_rpm = 0.0f;
float g_rpm_limit = kDefaultRpmLimit;
bool g_paused = true;
unsigned g_reconnect_frames = 0;
unsigned char g_last_mask = 0xff;
bool g_startup_animation_active = false;
bool g_startup_animation_completed = false;
unsigned g_startup_animation_step = 0;
ULONGLONG g_next_animation_tick = 0;

void log_message(const char* message)
{
    if (g_log) {
        g_log(SCS_LOG_TYPE_message, message);
    }
}

void log_warning(const char* message)
{
    if (g_log) {
        g_log(SCS_LOG_TYPE_warning, message);
    }
}

void close_wheel()
{
    if (g_wheel != INVALID_HANDLE_VALUE) {
        CloseHandle(g_wheel);
        g_wheel = INVALID_HANDLE_VALUE;
    }
    g_output_report_length = 0;
    g_last_mask = 0xff;
}

bool open_g29()
{
    close_wheel();

    GUID hid_guid{};
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO devices = SetupDiGetClassDevsW(
        &hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool opened = false;
    for (DWORD index = 0; !opened; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);
        if (!SetupDiEnumDeviceInterfaces(devices, nullptr, &hid_guid, index, &interface_data)) {
            break;
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(
            devices, &interface_data, nullptr, 0, &required, nullptr);
        if (!required) {
            continue;
        }

        std::vector<unsigned char> storage(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(
                devices, &interface_data, detail, required, nullptr, nullptr)) {
            continue;
        }

        HANDLE candidate = CreateFileW(
            detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (candidate == INVALID_HANDLE_VALUE) {
            continue;
        }

        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        HIDP_CAPS caps{};
        const bool is_g29 = HidD_GetAttributes(candidate, &attributes) &&
            attributes.VendorID == kLogitechVendorId &&
            attributes.ProductID == kG29ProductId;
        const bool has_output = HidD_GetPreparsedData(candidate, &preparsed) &&
            HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS &&
            caps.OutputReportByteLength >= 8;
        if (preparsed) {
            HidD_FreePreparsedData(preparsed);
        }

        if (is_g29 && has_output) {
            g_wheel = candidate;
            g_output_report_length = caps.OutputReportByteLength;
            g_last_mask = 0xff;
            opened = true;
        } else {
            CloseHandle(candidate);
        }
    }

    SetupDiDestroyDeviceInfoList(devices);
    return opened;
}

bool set_leds(unsigned char mask)
{
    mask &= 0x1f;
    if (mask == g_last_mask && g_wheel != INVALID_HANDLE_VALUE) {
        return true;
    }
    if (g_wheel == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Windows requires the buffer to be exactly OutputReportByteLength bytes,
    // even though Logitech's command itself occupies only eight bytes. HIDAPI
    // pads this automatically; here we do it explicitly.
    std::vector<unsigned char> report(g_output_report_length, 0x00);
    report[0] = 0x00; // HID report ID
    report[1] = 0xf8;
    report[2] = 0x12;
    report[3] = mask;

    DWORD written = 0;
    const bool write_ok = WriteFile(
        g_wheel, report.data(), static_cast<DWORD>(report.size()), &written, nullptr) &&
        written == report.size();
    const DWORD write_error = write_ok ? ERROR_SUCCESS : GetLastError();

    // Some Logitech/G HUB driver versions reject interrupt writes but accept
    // the equivalent HID control transfer.
    const bool control_ok = write_ok ||
        HidD_SetOutputReport(g_wheel, report.data(), static_cast<ULONG>(report.size()));
    const DWORD control_error = control_ok ? ERROR_SUCCESS : GetLastError();
    if (!control_ok) {
        char message[192]{};
        std::snprintf(
            message,
            sizeof(message),
            "G29 LED: output failed (report=%u, WriteFile=%lu, HidD=%lu)",
            static_cast<unsigned>(g_output_report_length),
            static_cast<unsigned long>(write_error),
            static_cast<unsigned long>(control_error));
        log_warning(message);
        close_wheel();
        return false;
    }

    g_last_mask = mask;
    return true;
}

unsigned char rpm_to_mask(float rpm, float limit)
{
    if (!std::isfinite(rpm) || !std::isfinite(limit) || limit < 100.0f) {
        return 0;
    }

    const float ratio = rpm / limit;
    if (ratio >= 0.96f) {
        // Flash all LEDs at about 8 Hz near the limiter.
        return ((GetTickCount64() / 62u) & 1u) ? 0x1f : 0x00;
    }
    if (ratio >= 0.90f) return 0x1f;
    if (ratio >= 0.82f) return 0x0f;
    if (ratio >= 0.73f) return 0x07;
    if (ratio >= 0.64f) return 0x03;
    if (ratio >= 0.55f) return 0x01;
    return 0x00;
}

void start_startup_animation()
{
    if (g_startup_animation_completed) {
        return;
    }
    g_startup_animation_active = true;
    g_startup_animation_step = 0;
    g_next_animation_tick = 0;
}

bool update_startup_animation()
{
    if (!g_startup_animation_active) {
        return false;
    }

    const ULONGLONG now = GetTickCount64();
    if (now < g_next_animation_tick) {
        return true;
    }

    constexpr unsigned steps_per_sweep = sizeof(kStartupSweep) / sizeof(kStartupSweep[0]);
    constexpr unsigned total_steps = steps_per_sweep * kStartupSweepCount;
    const unsigned char mask = kStartupSweep[g_startup_animation_step % steps_per_sweep];
    if (!set_leds(mask)) {
        g_startup_animation_active = false;
        return true;
    }

    ++g_startup_animation_step;
    if (g_startup_animation_step >= total_steps) {
        g_startup_animation_active = false;
        g_startup_animation_completed = true;
        set_leds(0);
        log_message("G29 LED: startup animation completed");
    } else {
        g_next_animation_tick = now + kAnimationStepMs;
    }
    return true;
}

void SCSAPIFUNC on_rpm(
    const scs_string_t, const scs_u32_t, const scs_value_t* value, const scs_context_t)
{
    if (value && value->type == SCS_VALUE_TYPE_float) {
        g_rpm = value->value_float.value;
    }
}

void SCSAPIFUNC on_configuration(
    const scs_event_t, const void* event_info, const scs_context_t)
{
    const auto* info = static_cast<const scs_telemetry_configuration_t*>(event_info);
    if (!info || !info->id || std::strcmp(info->id, SCS_TELEMETRY_CONFIG_truck) != 0) {
        return;
    }

    for (const scs_named_value_t* item = info->attributes; item && item->name; ++item) {
        if (std::strcmp(item->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_rpm_limit) == 0 &&
            item->value.type == SCS_VALUE_TYPE_float &&
            item->value.value_float.value >= 100.0f) {
            g_rpm_limit = item->value.value_float.value;
            break;
        }
    }
}

void SCSAPIFUNC on_pause_state(
    const scs_event_t event, const void*, const scs_context_t)
{
    g_paused = event == SCS_TELEMETRY_EVENT_paused;
    if (g_paused) {
        set_leds(0);
    }
}

void SCSAPIFUNC on_frame_end(
    const scs_event_t, const void*, const scs_context_t)
{
    if (g_wheel == INVALID_HANDLE_VALUE) {
        if (++g_reconnect_frames < 300) {
            return;
        }
        g_reconnect_frames = 0;
        if (open_g29()) {
            const unsigned char mask = g_paused ? 0 : rpm_to_mask(g_rpm, g_rpm_limit);
            if (set_leds(mask)) {
                log_message("G29 LED: wheel reconnected");
                start_startup_animation();
            }
        } else {
            return;
        }
    }

    if (update_startup_animation()) {
        return;
    }

    if (!g_paused) {
        set_leds(rpm_to_mask(g_rpm, g_rpm_limit));
    }
}

} // namespace

extern "C" __declspec(dllexport) scs_result_t SCSAPIFUNC scs_telemetry_init(
    const scs_u32_t version, const scs_telemetry_init_params_t* params)
{
    if (version != SCS_TELEMETRY_VERSION_1_01 || !params) {
        return SCS_RESULT_unsupported;
    }

    const auto* api = static_cast<const scs_telemetry_init_params_v101_t*>(params);
    g_log = api->common.log;

    if (!api->common.game_id || std::strcmp(api->common.game_id, SCS_GAME_ID_EUT2) != 0) {
        log_warning("G29 LED: this plugin supports Euro Truck Simulator 2 only");
        return SCS_RESULT_unsupported;
    }

    g_rpm = 0.0f;
    g_rpm_limit = kDefaultRpmLimit;
    g_paused = true;
    g_reconnect_frames = 0;
    g_startup_animation_active = false;
    g_startup_animation_completed = false;
    g_startup_animation_step = 0;
    g_next_animation_tick = 0;

    const bool registered =
        api->register_for_event(SCS_TELEMETRY_EVENT_frame_end, on_frame_end, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_paused, on_pause_state, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_started, on_pause_state, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_configuration, on_configuration, nullptr) == SCS_RESULT_ok &&
        api->register_for_channel(
            SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm,
            SCS_U32_NIL,
            SCS_VALUE_TYPE_float,
            SCS_TELEMETRY_CHANNEL_FLAG_none,
            on_rpm,
            nullptr) == SCS_RESULT_ok;

    if (!registered) {
        log_warning("G29 LED: failed to register ETS2 telemetry callbacks");
        return SCS_RESULT_generic_error;
    }

    if (open_g29()) {
        if (set_leds(0)) {
            char message[128]{};
            std::snprintf(
                message,
                sizeof(message),
                "G29 LED: wheel detected; RPM LEDs enabled (report=%u)",
                static_cast<unsigned>(g_output_report_length));
            log_message(message);
            start_startup_animation();
        }
    } else {
        log_warning("G29 LED: wheel not found; will retry while driving");
    }

    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport) void SCSAPIFUNC scs_telemetry_shutdown()
{
    set_leds(0);
    close_wheel();
    g_log = nullptr;
}

