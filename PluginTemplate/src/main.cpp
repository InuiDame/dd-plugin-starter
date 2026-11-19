#include "PCH.h"

// 旧式 DLL 协议：加载器只 LoadLibraryW，不要求任何导出函数，生效全靠 DllMain。
// 新协议（DDPlugin_Query / DDPlugin_Load）需要导出纯 C 函数，本模板不涉及。

BOOL APIENTRY DllMain(HMODULE a_hModule, DWORD a_ulReason, LPVOID a_lpReserved)
{
	(void)a_hModule;
	(void)a_lpReserved;

	if (a_ulReason == DLL_PROCESS_ATTACH)
	{
		// debug 构建等待调试器附加，方便下断点。
		// 注意：这会卡住游戏启动，只在 debug 模式下有意义。
#ifndef NDEBUG
		while (!IsDebuggerPresent())
		{
			Sleep(100);
		}
#endif

		demo::log::Init(std::string{ Plugin::NAME }, Plugin::Version);

		// 后续在这里串起来：读 cfg → 校验 → 装 hook。
		// 校验失败时只跳过对应功能，不要直接 return 掉整个安装流程。

		LOG_INFO("%s attached (v%s)", std::string{ Plugin::NAME }.c_str(), PLUGIN_VERSION_STRING);
	}

	return TRUE;
}
