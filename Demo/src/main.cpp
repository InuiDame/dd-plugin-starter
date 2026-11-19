#include "PCH.h"

#include "Hooks.h"

// DllMain 是加载器进入插件的唯一入口（旧式 DLL 协议）。
// 两个硬约束：
//   1. DllMain 里不要做耗时操作（这里只读 cfg + 扫一次内存）；
//   2. 绝不让异常越过 DllMain 边界，否则游戏会直接崩在加载阶段。

BOOL APIENTRY DllMain(HMODULE a_hModule, DWORD a_ulReason, LPVOID a_lpReserved)
{
	(void)a_hModule;
	(void)a_lpReserved;

	if (a_ulReason != DLL_PROCESS_ATTACH)
	{
		return TRUE;
	}

#ifndef NDEBUG
	// debug 构建：等调试器附加，方便在第一行下断点。发布版没有这段。
	// 注意这会卡住进程启动，只在 debug 模式下有意义。
	while (!::IsDebuggerPresent())
	{
		::Sleep(100);
	}
#endif

	demo::log::Init(std::string{ Plugin::NAME }, Plugin::Version);
	LOG_INFO("%s v%s loading", std::string{ Plugin::NAME }.c_str(), PLUGIN_VERSION_STRING);

	try
	{
		demo::hooks::Install();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("unhandled exception during install: %s", e.what());
	}
	catch (...)
	{
		LOG_ERROR("unhandled non-standard exception during install");
	}

	return TRUE;
}
