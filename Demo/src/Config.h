#pragma once

// 极简 cfg：key = value，支持 # / // / ; 三种注释。
// 约定（来自 DD.Plugins 的实践）：
//   1. 缺键补默认值，未知键忽略（老 cfg 不会因为新增键而失效）；
//   2. 非法值 → 保留默认值 + 记一条 warning，绝不静默吞掉；
//   3. 首次生成模板，之后绝不覆盖玩家配置。

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace demo::patch
{
struct Config
{
	bool enabled{ true };
	std::string pattern;     // 特征码，例如 "48 8B ?? E8"。留空 = 不装任何补丁
	std::string replacement; // 匹配处要写入的字节，例如 "90 90 90"
	bool logEverySecond{ true };

	// 三个条件全满足才算真正启用，避免"只填了一半"时误动作。
	bool IsActive() const
	{
		return enabled && !pattern.empty() && !replacement.empty();
	}
};

struct LoadResult
{
	Config value;
	std::vector<std::string> warnings;
	std::filesystem::path source;
	bool existed{ false };  // 读之前文件是否已存在
	bool created{ false };  // 本次是否生成了模板
};

// <exe 目录>\DDModLoader\cfg\<插件名>.cfg
std::filesystem::path DefaultConfigPath(std::string_view pluginName);

LoadResult LoadConfig(const std::filesystem::path& file);
} // namespace demo::patch
