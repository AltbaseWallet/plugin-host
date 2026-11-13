#include "../plugin-sdk/altbase_plugin.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
constexpr size_t input_limit = 2 * 1024 * 1024;
constexpr size_t output_limit = 8 * 1024 * 1024;
bool line(std::string& value, size_t limit) {
  value.clear(); char c;
  while (std::cin.get(c)) {
    if (c == '\n') return true;
    if (value.size() >= limit || c == '\0' || c == '\r') return false;
    value.push_back(c);
  }
  return false;
}
bool output(const char* value, size_t limit) {
  if (!value) return false;
  size_t size = 0;
  while (size <= limit && value[size]) ++size;
  if (size > limit || std::strchr(value, '\n') || std::strchr(value, '\r')) return false;
  std::cout.write(value, static_cast<std::streamsize>(size));
  std::cout << '\n' << std::flush;
  return static_cast<bool>(std::cout);
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
  if (argc != 2) return 64;
  // Keep dependencies beside the plugin or in standard OS directories.
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  auto library = LoadLibraryExW(argv[1], nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  auto symbol = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(library, name)); };
#else
int main(int argc, char** argv) {
  if (argc != 2) return 64;
  auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  auto symbol = [&](const char* name) { return dlsym(library, name); };
#endif
  if (!library) return 65;
  using Version = uint32_t (ALTBASE_PLUGIN_CALL*)();
  using Manifest = const char* (ALTBASE_PLUGIN_CALL*)();
  using Request = char* (ALTBASE_PLUGIN_CALL*)(const char*, const char*);
  using Free = void (ALTBASE_PLUGIN_CALL*)(char*);
  auto version = reinterpret_cast<Version>(symbol("altbase_plugin_abi_version"));
  auto manifest = reinterpret_cast<Manifest>(symbol("altbase_plugin_manifest"));
  auto request = reinterpret_cast<Request>(symbol("altbase_plugin_request"));
  auto release = reinterpret_cast<Free>(symbol("altbase_plugin_free"));
  if (!version || !manifest || !request || !release || version() != 1) return 66;
  if (!output(manifest(), 512 * 1024)) return 67;
  std::string method, params;
  while (line(method, 64)) {
    if (method.empty()) return 68;
    for (unsigned char c : method) if (!std::isalnum(c) && c != '_') return 68;
    if (!line(params, input_limit)) return 69;
    char* result = request(method.c_str(), params.c_str());
    const bool sent = output(result, output_limit);
    if (result) release(result);
    if (!sent) return 70;
    // Best effort only; plugins own the lifecycle of any derived key material.
    std::fill(params.begin(), params.end(), '\0');
  }
  return 0;
}
