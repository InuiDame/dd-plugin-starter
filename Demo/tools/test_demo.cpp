// 离线单测：不启动游戏，也不需要真实 exe。
//
// 覆盖的是"最容易错、又最容易离线验证"的部分：
//   特征码解析 / 唯一命中判定 / 补丁前校验 / 字节写入 / cfg 解析与容错。
//
// 它不能证明的（必须实机验收）：补丁点在真实游戏里真的被执行、ASLR 后的
// 地址正确、UI 表现符合预期、与其他插件不冲突。离线全绿 ≠ 功能可用。

#include "Config.h"
#include "Patch.h"
#include "Scan.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;

void Check(bool condition, const char* name)
{
	if (condition)
	{
		std::printf("[PASS] %s\n", name);
	}
	else
	{
		std::printf("[FAIL] %s\n", name);
		++g_failures;
	}
}

std::vector<std::uint8_t> Bytes(std::initializer_list<int> values)
{
	std::vector<std::uint8_t> out;
	out.reserve(values.size());
	for (const int value : values)
	{
		out.push_back(static_cast<std::uint8_t>(value));
	}
	return out;
}

void TestPatternParse()
{
	std::string error;

	const auto ok = demo::scan::Pattern::Parse("48 8B ?? E8", error);
	Check(ok.has_value(), "parse: hex with wildcard is accepted");
	Check(ok && ok->Size() == 4, "parse: token count is 4");
	Check(ok && ok->mask[2] == 0 && ok->mask[0] == 1, "parse: wildcard becomes mask 0");
	Check(ok && ok->bytes[1] == 0x8B, "parse: byte value decoded");
	Check(ok && ok->Concrete()[2] == 0x00, "parse: wildcard concrete byte is 0");

	const auto question = demo::scan::Pattern::Parse("90 ? 90", error);
	Check(question.has_value() && question->Size() == 3, "parse: single '?' is a wildcard");

	const auto prefixed = demo::scan::Pattern::Parse("0x90 0x90", error);
	Check(prefixed.has_value() && prefixed->bytes[0] == 0x90, "parse: 0x prefix accepted");

	Check(!demo::scan::Pattern::Parse("", error).has_value(), "parse: empty string rejected");
	Check(!demo::scan::Pattern::Parse("zz", error).has_value(), "parse: non-hex rejected");
	Check(!demo::scan::Pattern::Parse("488B", error).has_value(), "parse: packed bytes without separator rejected");
	Check(!demo::scan::Pattern::Parse("4", error).has_value(), "parse: single nibble rejected");
}

void TestFindAll()
{
	std::string error;
	const auto pattern = demo::scan::Pattern::Parse("DE AD ?? EF", error);
	if (!pattern)
	{
		Check(false, "find: pattern fixture parsed");
		return;
	}

	const std::vector<std::uint8_t> unique = Bytes({ 0x00, 0xDE, 0xAD, 0x11, 0xEF, 0x00, 0x00 });
	const auto uniqueHits = demo::scan::FindAllInBuffer(unique.data(), unique.size(), *pattern);
	Check(uniqueHits.size() == 1, "find: unique hit reported once");
	Check(!uniqueHits.empty() && uniqueHits[0] == 1, "find: hit offset is correct");

	const std::vector<std::uint8_t> none = Bytes({ 0xDE, 0xAD, 0x11, 0xEE });
	Check(demo::scan::FindAllInBuffer(none.data(), none.size(), *pattern).empty(), "find: mismatched tail yields no hit");

	const std::vector<std::uint8_t> twice = Bytes({ 0xDE, 0xAD, 0x01, 0xEF, 0xDE, 0xAD, 0x02, 0xEF });
	const auto twiceHits = demo::scan::FindAllInBuffer(twice.data(), twice.size(), *pattern, 4);
	Check(twiceHits.size() == 2, "find: ambiguous pattern yields two hits");
	Check(twiceHits.size() == 2 && twiceHits[1] == 4, "find: second hit offset is correct");

	Check(demo::scan::FindAllInBuffer(twice.data(), twice.size(), *pattern, 1).size() == 1, "find: maxHits caps results");

	// 边界：数据比特征码短，不能越界读
	const std::vector<std::uint8_t> tiny = Bytes({ 0xDE, 0xAD });
	Check(demo::scan::FindAllInBuffer(tiny.data(), tiny.size(), *pattern).empty(), "find: buffer shorter than pattern is safe");

	// 重叠命中：AA AA AA 里找 AA AA 应命中 2 次
	const auto twice2 = demo::scan::Pattern::Parse("AA AA", error);
	const std::vector<std::uint8_t> overlap = Bytes({ 0xAA, 0xAA, 0xAA });
	Check(twice2 && demo::scan::FindAllInBuffer(overlap.data(), overlap.size(), *twice2, 8).size() == 2, "find: overlapping hits counted");
}

void TestVerifyAndApply()
{
	// 用堆内存模拟一个可写"代码段"，避免碰真实 exe
	std::vector<std::uint8_t> buffer = Bytes({ 0x8B, 0x45, 0x08, 0x90, 0x90 });

	demo::patch::Site site;
	site.address = reinterpret_cast<std::uintptr_t>(buffer.data());
	site.expected = Bytes({ 0x8B, 0x45, 0x08 });
	site.replacement = Bytes({ 0x33, 0xC0, 0x90 });

	std::string why;
	Check(demo::patch::Verify(site, why), "verify: matching bytes pass");

	const demo::patch::Result applied = demo::patch::Apply(site);
	Check(applied.ok, "apply: writable buffer patched");
	Check(buffer[0] == 0x33 && buffer[1] == 0xC0 && buffer[2] == 0x90, "apply: replacement bytes written");
	Check(buffer[3] == 0x90, "apply: bytes outside the site untouched");

	// 期望值与现场不符 → 必须拒绝，且一个字节都不能改
	std::vector<std::uint8_t> guarded = Bytes({ 0x11, 0x22, 0x33 });
	demo::patch::Site wrong;
	wrong.address = reinterpret_cast<std::uintptr_t>(guarded.data());
	wrong.expected = Bytes({ 0x8B, 0x45, 0x08 });
	wrong.replacement = Bytes({ 0x90, 0x90, 0x90 });
	Check(!demo::patch::Verify(wrong, why), "verify: mismatch is rejected");
	Check(why.find("byte 0 differs") != std::string::npos, "verify: mismatch message points at the byte");
	Check(!demo::patch::Apply(wrong).ok, "apply: mismatch is never written");
	Check(guarded[0] == 0x11 && guarded[1] == 0x22, "apply: rejected patch leaves memory intact");

	// 长度不一致必须直接拒绝
	demo::patch::Site badLength;
	badLength.address = reinterpret_cast<std::uintptr_t>(buffer.data());
	badLength.expected = Bytes({ 0x33, 0xC0, 0x90 });
	badLength.replacement = Bytes({ 0x90 });
	Check(!demo::patch::Verify(badLength, why), "verify: length mismatch rejected");

	// mask 为 0 的位置不参与比较（对应特征码里的 ??）
	std::vector<std::uint8_t> masked = Bytes({ 0xAA, 0x99, 0xCC });
	demo::patch::Site withMask;
	withMask.address = reinterpret_cast<std::uintptr_t>(masked.data());
	withMask.expected = Bytes({ 0xAA, 0x00, 0xCC });
	withMask.mask = Bytes({ 1, 0, 1 });
	withMask.replacement = Bytes({ 0x90, 0x90, 0x90 });
	Check(demo::patch::Verify(withMask, why), "verify: wildcard position is skipped");
	Check(demo::patch::Apply(withMask).ok && masked[1] == 0x90, "apply: masked site still writes full replacement");
}

void TestConfig()
{
	const std::filesystem::path dir = std::filesystem::temp_directory_path() / "dd_starter_test";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);

	const std::filesystem::path file = dir / "Demo.cfg";

	{
		std::ofstream out{ file, std::ios::binary | std::ios::trunc };
		out << "# comment line\n"
			<< "enabled = 1\n"
			<< "pattern = 48 8B ?? E8   ; trailing comment\n"
			<< "replacement = 33 C0 90 90\n"
			<< "log_every_second = 0\n"
			<< "unknown_key = whatever\n";
	}

	const demo::patch::LoadResult loaded = demo::patch::LoadConfig(file);
	Check(loaded.existed && !loaded.created, "config: existing file is not overwritten");
	Check(loaded.value.enabled, "config: enabled parsed");
	Check(loaded.value.pattern == "48 8B ?? E8", "config: pattern trimmed and comment stripped");
	Check(loaded.value.replacement == "33 C0 90 90", "config: replacement parsed");
	Check(!loaded.value.logEverySecond, "config: bool false parsed");
	Check(loaded.warnings.empty(), "config: unknown key ignored silently");
	Check(loaded.value.IsActive(), "config: complete config is active");

	{
		std::ofstream out{ file, std::ios::binary | std::ios::trunc };
		out << "enabled = maybe\n"
			<< "pattern =\n";
	}
	const demo::patch::LoadResult bad = demo::patch::LoadConfig(file);
	Check(bad.value.enabled, "config: invalid bool keeps default");
	Check(bad.warnings.size() == 1, "config: invalid bool produces one warning");
	Check(!bad.value.IsActive(), "config: empty pattern is inactive");

	const demo::patch::LoadResult missing = demo::patch::LoadConfig(dir / "Missing.cfg");
	Check(missing.created, "config: missing file generates template");
	Check(missing.value.enabled && missing.value.pattern.empty(), "config: template defaults are safe");
	Check(!missing.value.IsActive(), "config: freshly generated template does nothing");

	std::filesystem::remove_all(dir, ec);
}
} // namespace

int main()
{
	std::printf("== dd-plugin-starter offline tests ==\n");

	TestPatternParse();
	TestFindAll();
	TestVerifyAndApply();
	TestConfig();

	if (g_failures == 0)
	{
		std::printf("== ALL PASS ==\n");
		return 0;
	}

	std::printf("== %d FAILURE(S) ==\n", g_failures);
	return 1;
}
