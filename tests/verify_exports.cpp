#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fputs("usage: verify_exports <plugin.dll>\n", stderr);
        return 2;
    }

    HMODULE plugin = LoadLibraryA(argv[1]);
    if (!plugin) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }

    const bool has_init = GetProcAddress(plugin, "scs_telemetry_init") != nullptr;
    const bool has_shutdown = GetProcAddress(plugin, "scs_telemetry_shutdown") != nullptr;
    std::printf("scs_telemetry_init=%s\n", has_init ? "OK" : "MISSING");
    std::printf("scs_telemetry_shutdown=%s\n", has_shutdown ? "OK" : "MISSING");
    FreeLibrary(plugin);
    return has_init && has_shutdown ? 0 : 1;
}
