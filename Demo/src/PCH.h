#pragma once

// 预编译头：把稳定不变的重头文件集中在这里，缩短编译时间。
// 原则：只放标准库 / Windows / 插件公共头，不放会频繁改动的本项目头文件。

// c
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// cxx
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// windows
#include <windows.h>

// 构建期生成的插件元信息（PLUGIN_NAME / AUTHOR / VERSION_*），
// 由 xmake/Plugin.h.in 生成到 autogen 目录，不要手写同名文件。
#include "Plugin.h"

// 仓库公共设施
#include "Log.h"
