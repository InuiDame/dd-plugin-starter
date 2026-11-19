#pragma once

// 极简日志：写 <exe 目录>\DDModLoader\log\<插件名>.log，失败时退回 %LOCALAPPDATA%。
//
// 为什么不用 spdlog：starter 要保证离线、零外部依赖即可构建。
// 想换成 DKUtil / spdlog 时，只需要改这一个 .cpp，宏接口保持不变。

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace demo::log
{
enum class Level
{
	Info,
	Warn,
	Error,
};

// 只调用一次，通常在 DllMain(DLL_PROCESS_ATTACH) 开头。
void Init(std::string_view pluginName, uint32_t version);

void Write(Level level, const char* format, ...);

// 日志节流：返回 true 的频率最多每秒一次。
// 用在每帧/每次调用都会经过的热路径上，避免写爆磁盘。
bool DiagDue();

// 当前日志文件路径（诊断用，Init 之后有效）。
const std::filesystem::path& Path();
} // namespace demo::log

// 宏名带 LOG_ 前缀：Windows SDK 的 wingdi.h 已经定义了 ERROR，
// 直接用 INFO/WARN/ERROR 会撞宏（C4005）。
#define LOG_INFO(...) ::demo::log::Write(::demo::log::Level::Info, __VA_ARGS__)
#define LOG_WARN(...) ::demo::log::Write(::demo::log::Level::Warn, __VA_ARGS__)
#define LOG_ERROR(...) ::demo::log::Write(::demo::log::Level::Error, __VA_ARGS__)
