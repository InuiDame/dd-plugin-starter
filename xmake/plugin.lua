set_xmakever("2.8.5")
set_allowedplats("windows")
set_allowedarchs("x64")
set_defaultarchs("windows|x64")
set_defaultmode("releasedbg")

-- 与 DD.Plugins 的差异：这里不 add_requires 任何包，保证离线可构建。
-- 需要 xbyak / spdlog / nlohmann_json 时，在对应插件的 xmake.lua 里自行 add_requires。

rule("dd.plugin")
	add_deps("mode.debug", "mode.releasedbg", "mode.release")

	on_load(function (target)
		local root = path.directory(os.scriptdir())

		-- DKUtil 是可选的：找到就用，找不到不报错。
		-- 查找顺序与 DD.Plugins 一致：<插件>/extern → <根>/extern → $env:DKUtilPath
		local dkutil
		for _, dir in ipairs({
			path.join(target:scriptdir(), "extern", "DKUtil"),
			path.join(root, "extern", "DKUtil"),
			os.getenv("DKUtilPath") or "",
		}) do
			if dir ~= "" and os.isfile(path.join(dir, "include", "DKUtil", "Hook.hpp")) then
				dkutil = path.absolute(dir)
				break
			end
		end

		target:set("kind", target:values("dd.kind") or "shared")
		target:set("languages", "c++23")
		target:set("runtimes", is_mode("debug") and "MTd" or "MT")
		target:set("exceptions", "cxx")
		target:set("warnings", "all")

		if dkutil then
			target:data_set("dd.dkutil", dkutil)
			target:add("includedirs", path.join(dkutil, "include"))
			target:add("defines", "DD_HAVE_DKUTIL")
		else
			cprint("${yellow}note${clear} %s: DKUtil not found, building without it", target:name())
		end

		-- 仓库级公共设施（common/Log.h/.cpp）：所有插件共用一份日志实现。
		-- 放在规则里注入，新插件就不必各自复制或改 xmake.lua。
		local common = path.join(root, "common")
		target:add("includedirs", common)
		target:add("files", path.join(common, "Log.cpp"))

		target:add("syslinks", "ole32", "shell32", "user32", "version")		target:add("defines", "UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
		target:add("cxflags",
			"/utf-8",
			"/permissive-",
			"/Zc:__cplusplus",
			"/Zc:preprocessor",
			"/Zc:alignedNew",
			"/Zc:auto",
			"/Zc:externConstexpr",
			"/Zc:forScope",
			"/Zc:hiddenFriend",
			"/Zc:implicitNoexcept",
			"/Zc:lambda",
			"/Zc:noexceptTypes",
			"/Zc:referenceBinding",
			"/Zc:rvalueCast",
			"/Zc:sizedDealloc",
			"/Zc:strictStrings",
			"/Zc:ternary",
			"/Zc:threadSafeInit",
			"/Zc:wchar_t",
			"/wd4200",
			{ tools = "cl" })

		-- 构建期生成 Plugin.h（含 NAME / AUTHOR / 版本），源码里 #include "Plugin.h"。
		-- 不要手写同名文件覆盖它。
		local configdir = path.join(target:autogendir(), "config")
		target:set("configdir", configdir)
		target:add("configfiles", path.join(root, "xmake", "Plugin.h.in"), { filename = "Plugin.h" })
		target:set("configvar", "PLUGIN_NAME", target:name())
		target:set("configvar", "PLUGIN_AUTHOR", os.getenv("DDPluginAuthor") or "")
		target:set("configvar", "PLUGIN_LICENSE", target:values("dd.license") or "MIT License")
		target:add("includedirs", configdir)
		target:add("files", path.join(root, "xmake", "version.rc"))
	end)

	-- 部署门控：DDPath 存在 且 dd.mod 非空 且 DD_DEPLOY=1 三个条件同时满足才复制。
	-- 为什么要第三道门：本机 DDPath 可能是用户级环境变量并指向真实安装，
	-- 只靠 DDPath 会在 `xmake` 全量构建时把 DLL 悄悄写进真实游戏目录。
	-- 不做部署时，构建对游戏目录零副作用。
	after_build(function (target)
		local game = os.getenv("DDPath")
		local mod = target:values("dd.mod")
		local allow = os.getenv("DD_DEPLOY") == "1"
		if not allow or not game or game == "" or not mod then
			return
		end

		local dir = path.join(game, "mods", mod)
		os.mkdir(dir)
		os.cp(target:targetfile(), dir)
		cprint("${bright green}deployed${clear} %s -> %s", path.filename(target:targetfile()), dir)
	end)
rule_end()
