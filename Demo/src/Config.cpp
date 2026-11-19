#include "Config.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace demo::patch
{
namespace
{
std::string Trim(std::string s)
{
	const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };

	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::string StripComment(const std::string& line)
{
	std::string out = line;
	const std::size_t hash = out.find('#');
	if (hash != std::string::npos)
	{
		out.erase(hash);
	}
	const std::size_t slash = out.find("//");
	if (slash != std::string::npos)
	{
		out.erase(slash);
	}
	const std::size_t semi = out.find(';');
	if (semi != std::string::npos)
	{
		out.erase(semi);
	}
	return out;
}

bool ParseBool(const std::string& raw, bool& out)
{
	const std::string v = Lower(Trim(raw));
	if (v == "1" || v == "true" || v == "yes" || v == "on")
	{
		out = true;
		return true;
	}
	if (v == "0" || v == "false" || v == "no" || v == "off")
	{
		out = false;
		return true;
	}
	return false;
}

constexpr const char* kTemplate = R"(# Demo 插件配置
# 改完需重启游戏才生效。文件位置：<游戏目录>\DDModLoader\cfg\Demo.cfg

# 总开关。0 = 连特征码都不扫描，完全不碰游戏内存
enabled = 1

# 要定位的特征码（十六进制字节，?? 或 ? 表示通配）
# 留空 = 不装补丁，只写一条启动日志。先留空跑通加载，再填真实特征码。
pattern =

# 命中之后要写进内存的字节，必须和 pattern 的字节数完全相同
replacement =

# 每秒最多一条 [DIAG] 日志，避免热路径写爆磁盘
log_every_second = 1
)";
} // namespace

std::filesystem::path DefaultConfigPath(std::string_view pluginName)
{
	wchar_t buffer[MAX_PATH]{};
	const DWORD len = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	if (len == 0 || len == MAX_PATH)
	{
		return {};
	}
	return std::filesystem::path{ buffer }.parent_path() / L"DDModLoader" / L"cfg" /
		(std::string{ pluginName } + ".cfg");
}

LoadResult LoadConfig(const std::filesystem::path& file)
{
	LoadResult result;
	result.source = file;

	std::error_code ec;
	result.existed = std::filesystem::exists(file, ec) && !ec;

	if (!result.existed && !file.empty())
	{
		// 首次生成模板；已存在时绝不覆盖，玩家改过的值必须保留。
		std::filesystem::create_directories(file.parent_path(), ec);
		if (!ec)
		{
			std::ofstream out{ file, std::ios::binary | std::ios::trunc };
			if (out)
			{
				out << kTemplate;
				out.close();
				result.created = out.good();
			}
		}
		if (!result.created)
		{
			result.warnings.emplace_back("config template could not be created: " + file.string());
			return result;
		}
	}

	std::ifstream in{ file, std::ios::binary };
	if (!in)
	{
		result.warnings.emplace_back("config not readable: " + file.string());
		return result;
	}

	std::string line;
	std::size_t lineNo = 0;
	while (std::getline(in, line))
	{
		++lineNo;
		const std::string body = Trim(StripComment(line));
		if (body.empty())
		{
			continue;
		}

		const std::size_t eq = body.find('=');
		if (eq == std::string::npos)
		{
			result.warnings.emplace_back("line " + std::to_string(lineNo) + ": missing '=', ignored");
			continue;
		}

		const std::string key = Lower(Trim(body.substr(0, eq)));
		const std::string value = Trim(body.substr(eq + 1));

		if (key == "enabled")
		{
			if (!ParseBool(value, result.value.enabled))
			{
				result.warnings.emplace_back("line " + std::to_string(lineNo) + ": enabled='" + value + "' is not a bool, kept default");
			}
		}
		else if (key == "log_every_second")
		{
			if (!ParseBool(value, result.value.logEverySecond))
			{
				result.warnings.emplace_back("line " + std::to_string(lineNo) + ": log_every_second='" + value + "' is not a bool, kept default");
			}
		}
		else if (key == "pattern")
		{
			result.value.pattern = value;
		}
		else if (key == "replacement")
		{
			result.value.replacement = value;
		}
		// 未知键直接忽略：旧版 DLL 读新 cfg 不会炸，反之亦然。
	}

	return result;
}
} // namespace demo::patch
