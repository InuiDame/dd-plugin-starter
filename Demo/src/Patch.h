#pragma once

// 字节补丁：先验证、再写入。验证不通过一律不写。
//
// 这套"全有或全无"（all-or-nothing）是 DD.Plugins 里最核心的安全约束：
// 游戏一更新，字节就对不上；此时如果继续无脑写，轻则无效，重则崩游戏。
// 正确做法是——对不上就什么都不做，只在日志里说明哪一项失败。

#include <cstdint>
#include <string>
#include <vector>

namespace demo::patch
{
struct Site
{
	std::uintptr_t address{};

	// 补丁前应该是什么。与 mask 等长，通配位置由 mask 标记为不比较。
	std::vector<std::uint8_t> expected;

	// 0 = 该位置不比较（对应特征码里的 ??），1 = 必须相等。
	// 留空等价于"全部比较"。
	std::vector<std::uint8_t> mask;

	// 要写成什么，长度必须与 expected 相同。
	std::vector<std::uint8_t> replacement;
};

struct Result
{
	bool ok{ false };
	std::string error;
};

// 只读校验，不改内存。失败时 why 给出具体原因（含第几个字节不符）。
bool Verify(const Site& site, std::string& why);

// 校验通过才写；写入时临时改页属性，写完恢复并刷新指令缓存。
Result Apply(const Site& site);
} // namespace demo::patch
