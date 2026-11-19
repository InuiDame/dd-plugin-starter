#pragma once

// 特征码扫描：把 "48 8B ?? E8" 这样的文本解析成字节 + 掩码，然后在
//   - 一段连续内存（离线单测用）
//   - 主模块的可提交内存区间（运行时用）
// 里找命中位置。
//
// 铁律：只认"唯一命中"。0 个命中说明版本不符，>1 个命中说明特征码太短，
// 两种情况都必须放弃安装，而不是随便挑第一个。

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace demo::scan
{
struct Pattern
{
	std::vector<std::uint8_t> bytes;
	std::vector<std::uint8_t> mask; // 0 = 通配（不比较），1 = 必须相等

	// 解析失败时把原因写进 error，返回 nullopt。
	static std::optional<Pattern> Parse(std::string_view text, std::string& error);

	bool Matches(const std::uint8_t* at) const;
	std::size_t Size() const { return bytes.size(); }
	bool Empty() const { return bytes.empty(); }

	// 通配位置填 0 后的字节串，用作"补丁前应该是什么"的期望值。
	std::vector<std::uint8_t> Concrete() const;
};

struct ModuleInfo
{
	std::uintptr_t base{};
	std::size_t size{};
};

std::optional<ModuleInfo> MainModule();

// 返回命中处的偏移（相对 data 起点），最多记录 maxHits 个。
std::vector<std::uintptr_t> FindAllInBuffer(const std::uint8_t* data, std::size_t size, const Pattern& pattern, std::size_t maxHits = 8);

// 返回绝对地址（base + 偏移）。
std::vector<std::uintptr_t> FindAllInModule(const ModuleInfo& module, const Pattern& pattern, std::size_t maxHits = 8);
} // namespace demo::scan
