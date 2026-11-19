#include "PCH.h"

#include "Hooks.h"

#include "Config.h"
#include "Patch.h"
#include "Scan.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace demo::hooks
{
namespace
{
// 无条件计数器：不管功能是否生效，每次经过都 +1。
// 出问题时先看这些数字，才能区分"没跑到"和"跑到了但判断错"。
std::atomic<std::uint32_t> g_scans{ 0 };
std::atomic<std::uint32_t> g_hits{ 0 };
std::atomic<std::uint32_t> g_applied{ 0 };
std::atomic<bool> g_diagRunning{ false };

std::string ToHex(std::uintptr_t value)
{
	char buffer[32]{};
	std::snprintf(buffer, sizeof buffer, "0x%llX", static_cast<unsigned long long>(value));
	return buffer;
}

void DiagLoop()
{
	while (g_diagRunning.load(std::memory_order_relaxed))
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (!demo::log::DiagDue())
		{
			continue;
		}

		LOG_INFO("[DIAG] scans=%u hits=%u applied=%u",
			g_scans.load(std::memory_order_relaxed),
			g_hits.load(std::memory_order_relaxed),
			g_applied.load(std::memory_order_relaxed));
	}
}

bool ParseLiteralBytes(std::string_view text, std::vector<std::uint8_t>& out, std::string& error)
{
	std::string parseError;
	const auto pattern = demo::scan::Pattern::Parse(text, parseError);
	if (!pattern)
	{
		error = parseError;
		return false;
	}
	for (const std::uint8_t mask : pattern->mask)
	{
		if (mask == 0)
		{
			error = "replacement must not contain wildcards";
			return false;
		}
	}
	out = pattern->bytes;
	return true;
}
} // namespace

void Install()
{
	const std::string pluginName{ Plugin::NAME };

	const std::filesystem::path configPath = demo::patch::DefaultConfigPath(pluginName);
	demo::patch::LoadResult loaded = demo::patch::LoadConfig(configPath);

	if (loaded.created)
	{
		LOG_INFO("config template created: %s", configPath.string().c_str());
	}
	else if (!loaded.existed)
	{
		LOG_WARN("config missing and could not be created, running with defaults: %s", configPath.string().c_str());
	}
	for (const std::string& warning : loaded.warnings)
	{
		LOG_WARN("config: %s", warning.c_str());
	}

	const demo::patch::Config& config = loaded.value;

	if (!config.enabled)
	{
		LOG_INFO("disabled by config, nothing installed");
		return;
	}

	if (config.pattern.empty() || config.replacement.empty())
	{
		// 模板默认是这个状态：证明"插件被加载 + 日志工作正常"，但不碰游戏内存。
		LOG_INFO("no pattern configured (safe no-op). fill pattern/replacement in %s", configPath.string().c_str());
		return;
	}

	demo::scan::Pattern pattern;
	{
		std::string error;
		const auto parsed = demo::scan::Pattern::Parse(config.pattern, error);
		if (!parsed)
		{
			LOG_ERROR("pattern invalid: %s", error.c_str());
			return;
		}
		pattern = *parsed;
	}

	std::vector<std::uint8_t> replacement;
	{
		std::string error;
		if (!ParseLiteralBytes(config.replacement, replacement, error))
		{
			LOG_ERROR("replacement invalid: %s", error.c_str());
			return;
		}
	}

	if (replacement.size() != pattern.Size())
	{
		LOG_ERROR("length mismatch: pattern has %zu bytes, replacement has %zu",
			pattern.Size(), replacement.size());
		return;
	}

	const auto module = demo::scan::MainModule();
	if (!module)
	{
		LOG_ERROR("failed to resolve main module image");
		return;
	}
	LOG_INFO("module base=%s size=%s", ToHex(module->base).c_str(),
		ToHex(static_cast<std::uintptr_t>(module->size)).c_str());

	++g_scans;
	const auto hits = demo::scan::FindAllInModule(*module, pattern, 4);

	if (hits.empty())
	{
		// 0 命中：版本不符，或特征码太短被优化改写。放弃安装，不要猜。
		LOG_ERROR("pattern not found (%zu bytes). game build probably changed; NOT patching", pattern.Size());
		return;
	}

	if (hits.size() > 1)
	{
		// >1 命中：特征码不唯一。随便挑一个就是在赌，必须加长特征码。
		LOG_ERROR("pattern is ambiguous (%zu hits). lengthen the signature; NOT patching", hits.size());
		for (const std::uintptr_t hit : hits)
		{
			LOG_WARN("  candidate RVA=%s", ToHex(hit - module->base).c_str());
		}
		return;
	}

	++g_hits;

	demo::patch::Site site;
	site.address = hits.front();
	site.expected = pattern.Concrete();
	site.mask = pattern.mask;
	site.replacement = replacement;

	const std::uintptr_t rva = site.address - module->base;
	LOG_INFO("unique hit at RVA=%s, verifying before write", ToHex(rva).c_str());

	const demo::patch::Result applied = demo::patch::Apply(site);
	if (!applied.ok)
	{
		// 只跳过本功能，不 return 掉整个流程；日志写清原因便于回传校准。
		LOG_ERROR("patch NOT applied: %s", applied.error.c_str());
		return;
	}

	++g_applied;
	LOG_INFO("patched %zu bytes at RVA=%s", site.replacement.size(), ToHex(rva).c_str());

	if (config.logEverySecond)
	{
		bool expected = false;
		if (g_diagRunning.compare_exchange_strong(expected, true))
		{
			// detach 的守护线程：DllMain 里不能等它退出，否则会和加载器锁互锁。
			std::thread{ DiagLoop }.detach();
		}
	}
}
} // namespace demo::hooks
