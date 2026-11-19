set_project("DD.PluginStarter")

-- 公共规则：所有插件都通过 add_rules("dd.plugin") 获得统一的编译选项、
-- 版本资源、Plugin.h 生成与（可选的）构建后部署。
includes("xmake/plugin.lua")

-- 自动发现每个插件目录。新增插件只需建目录 + 自己的 xmake.lua，不用改这里。
for _, file in ipairs(os.files(path.join(os.scriptdir(), "*", "xmake.lua"))) do
	local name = path.filename(path.directory(file))
	if name ~= "PluginTemplate" and name ~= "extern" and name ~= "xmake" then
		includes(file)
	end
end
