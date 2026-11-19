includes("../xmake/plugin.lua")

target("Demo")
	add_rules("dd.plugin")
	set_version("1.0.0")
	-- 声明部署目标：只有在设置了 DDPath 时才会复制到 <DDPath>\mods\DemoMod\
	set_values("dd.mod", "DemoMod")

	set_pcxxheader("src/PCH.h")
	add_files("src/**.cpp")
	add_headerfiles("src/**.h")
target_end()

-- 离线单测：不启动游戏、不需要 exe 也能验证扫描 / 校验 / 补丁 / cfg 逻辑。
-- 默认不构建，显式执行：xmake build demo_test
target("demo_test")
	set_kind("binary")
	set_default(false)
	set_group("Demo/tools")
	set_languages("c++23")
	set_runtimes(is_mode("debug") and "MTd" or "MT")
	add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE")
	add_cxflags("/utf-8", "/permissive-", "/W4", { tools = "cl" })
	add_includedirs("src")
	add_files("tools/test_demo.cpp", "src/Scan.cpp", "src/Patch.cpp", "src/Config.cpp")
target_end()
