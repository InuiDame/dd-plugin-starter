#include "Log.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace demo::log
{
namespace
{
std::mutex g_mutex;
std::FILE* g_file = nullptr;
std::filesystem::path g_path;
std::atomic<std::int64_t> g_lastDiagMs{ 0 };

std::int64_t NowMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::filesystem::path ExecutableDir()
{
	wchar_t buffer[MAX_PATH]{};
	const DWORD len = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (len == 0 || len == MAX_PATH)
	{
		return {};
	}
	return std::filesystem::path{ buffer }.parent_path();
}

std::filesystem::path PickLogDir()
{
	std::error_code ec;

	// 首选：与 DDModLoader 约定一致的位置，方便和加载器日志放在一起。
	std::filesystem::path dir = ExecutableDir() / L"DDModLoader" / L"log";
	if (!dir.empty())
	{
		std::filesystem::create_directories(dir, ec);
		if (!ec)
		{
			return dir;
		}
	}

	// 兜底：游戏目录可能不可写（装在 Program Files 下）。
	wchar_t* local = nullptr;
	std::size_t localLen = 0;
	if (_wdupenv_s(&local, &localLen, L"LOCALAPPDATA") == 0 && local)
	{
		std::filesystem::path fallback = std::filesystem::path{ local } / L"DDPluginStarter" / L"log";
		std::free(local);
		ec.clear();
		std::filesystem::create_directories(fallback, ec);
		if (!ec)
		{
			return fallback;
		}
	}
	return {};
}

const char* LevelTag(Level level)
{
	switch (level)
	{
	case Level::Info:
		return "INFO";
	case Level::Warn:
		return "WARN";
	case Level::Error:
		return "ERROR";
	}
	return "?";
}
} // namespace

void Init(std::string_view pluginName, uint32_t version)
{
	{
		// 注意：持锁期间不要调用 Write()，它会再拿一次同一把非递归锁。
		std::lock_guard lock{ g_mutex };
		if (g_file)
		{
			return;
		}

		const std::filesystem::path dir = PickLogDir();
		if (dir.empty())
		{
			return;
		}

		g_path = dir / (std::string{ pluginName } + ".log");

		// _wfopen_s + ccs=UTF-8：追加写入，避免多份 DLL 抢同一文件时的共享冲突。
		if (_wfopen_s(&g_file, g_path.wstring().c_str(), L"a, ccs=UTF-8") != 0)
		{
			g_file = nullptr;
			g_path.clear();
			return;
		}
	}

	// 格式串是 printf 风格（%s / %u），不是 std::format 的 {}。
	Write(Level::Info, "==== %s v%u log opened ====", std::string{ pluginName }.c_str(), version);
}

void Write(Level level, const char* format, ...)
{
	char message[1024]{};
	va_list args;
	va_start(args, format);
	_vsnprintf_s(message, sizeof message, _TRUNCATE, format, args);
	va_end(args);

	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &now);

	char stamp[32]{};
	std::strftime(stamp, sizeof stamp, "%H:%M:%S", &local);

	std::lock_guard lock{ g_mutex };
	if (!g_file)
	{
		::OutputDebugStringA(message);
		::OutputDebugStringA("\n");
		return;
	}

	std::fprintf(g_file, "[%s] [%s] %s\n", stamp, LevelTag(level), message);
	std::fflush(g_file);
}

bool DiagDue()
{
	const std::int64_t now = NowMs();
	std::int64_t last = g_lastDiagMs.load(std::memory_order_relaxed);
	if (now - last < 1000)
	{
		return false;
	}
	return g_lastDiagMs.compare_exchange_strong(last, now, std::memory_order_relaxed);
}

const std::filesystem::path& Path()
{
	return g_path;
}
} // namespace demo::log
