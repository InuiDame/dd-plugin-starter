#include "Scan.h"

#include <windows.h>

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace demo::scan
{
namespace
{
bool IsSpace(char c)
{
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::vector<std::string_view> SplitTokens(std::string_view text)
{
	std::vector<std::string_view> tokens;
	std::size_t i = 0;
	while (i < text.size())
	{
		while (i < text.size() && IsSpace(text[i]))
		{
			++i;
		}
		const std::size_t start = i;
		while (i < text.size() && !IsSpace(text[i]))
		{
			++i;
		}
		if (i > start)
		{
			tokens.push_back(text.substr(start, i - start));
		}
	}
	return tokens;
}

int HexValue(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}
	return -1;
}
} // namespace

std::optional<Pattern> Pattern::Parse(std::string_view text, std::string& error)
{
	Pattern pattern;

	for (std::string_view token : SplitTokens(text))
	{
		// 支持 "?" / "??" / "0x??" 三种通配写法
		if (token == "?" || token == "??")
		{
			pattern.bytes.push_back(0);
			pattern.mask.push_back(0);
			continue;
		}

		std::string_view digits = token;
		if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
		{
			digits.remove_prefix(2);
		}

		bool wildcard = digits.size() == 2 && (digits[0] == '?' || digits[1] == '?');
		if (wildcard)
		{
			pattern.bytes.push_back(0);
			pattern.mask.push_back(0);
			continue;
		}

		if (digits.size() != 2)
		{
			error = "token '" + std::string{ token } + "' is not a byte (expected two hex digits or ??)";
			return std::nullopt;
		}

		const int hi = HexValue(digits[0]);
		const int lo = HexValue(digits[1]);
		if (hi < 0 || lo < 0)
		{
			error = "token '" + std::string{ token } + "' contains non-hex characters";
			return std::nullopt;
		}

		pattern.bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
		pattern.mask.push_back(1);
	}

	if (pattern.bytes.empty())
	{
		error = "pattern is empty";
		return std::nullopt;
	}
	if (pattern.bytes.size() > 256)
	{
		error = "pattern longer than 256 bytes is not supported";
		return std::nullopt;
	}
	return pattern;
}

bool Pattern::Matches(const std::uint8_t* at) const
{
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		if (mask[i] != 0 && at[i] != bytes[i])
		{
			return false;
		}
	}
	return true;
}

std::vector<std::uint8_t> Pattern::Concrete() const
{
	std::vector<std::uint8_t> out(bytes.size(), 0);
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		if (mask[i] != 0)
		{
			out[i] = bytes[i];
		}
	}
	return out;
}

std::optional<ModuleInfo> MainModule()
{
	HMODULE module = ::GetModuleHandleW(nullptr);
	if (!module)
	{
		return std::nullopt;
	}

	// 从 PE 头取 SizeOfImage，而不是靠"摸到哪算哪"。
	const auto base = reinterpret_cast<const std::uint8_t*>(module);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return std::nullopt;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
	{
		return std::nullopt;
	}

	ModuleInfo info;
	info.base = reinterpret_cast<std::uintptr_t>(base);
	info.size = nt->OptionalHeader.SizeOfImage;
	return info;
}

std::vector<std::uintptr_t> FindAllInBuffer(const std::uint8_t* data, std::size_t size, const Pattern& pattern, std::size_t maxHits)
{
	std::vector<std::uintptr_t> hits;
	if (!data || pattern.Empty() || size < pattern.Size() || maxHits == 0)
	{
		return hits;
	}

	const std::size_t last = size - pattern.Size();
	for (std::size_t offset = 0; offset <= last; ++offset)
	{
		if (pattern.Matches(data + offset))
		{
			hits.push_back(offset);
			if (hits.size() >= maxHits)
			{
				break;
			}
		}
	}
	return hits;
}

std::vector<std::uintptr_t> FindAllInModule(const ModuleInfo& module, const Pattern& pattern, std::size_t maxHits)
{
	std::vector<std::uintptr_t> hits;
	if (module.base == 0 || module.size == 0 || pattern.Empty())
	{
		return hits;
	}

	const std::size_t overlap = pattern.Size() > 1 ? pattern.Size() - 1 : 0;

	// 用 carry 保存上一块内存的尾部，避免特征码正好跨页 / 跨区时漏检。
	std::vector<std::uint8_t> carry;
	std::uintptr_t address = module.base;
	const std::uintptr_t end = module.base + module.size;

	while (address < end)
	{
		MEMORY_BASIC_INFORMATION mbi{};
		if (::VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof mbi) == 0)
		{
			break;
		}

		std::size_t regionSize = static_cast<std::size_t>(mbi.RegionSize);
		if (address + regionSize > end)
		{
			regionSize = static_cast<std::size_t>(end - address);
		}

		const bool readable = mbi.State == MEM_COMMIT &&
			(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;

		if (readable && regionSize > 0)
		{
			const auto* region = reinterpret_cast<const std::uint8_t*>(address);

			// carry + 本区块组成一块连续数据来扫描
			std::vector<std::uint8_t> window;
			window.reserve(carry.size() + regionSize);
			window.insert(window.end(), carry.begin(), carry.end());
			window.insert(window.end(), region, region + regionSize);

			const std::vector<std::uintptr_t> local = FindAllInBuffer(window.data(), window.size(), pattern, maxHits - hits.size());
			for (const std::uintptr_t offset : local)
			{
				const std::uintptr_t absolute = address - carry.size() + offset;
				hits.push_back(absolute);
				if (hits.size() >= maxHits)
				{
					return hits;
				}
			}

			const std::size_t keep = window.size() < overlap ? window.size() : overlap;
			carry.assign(window.end() - static_cast<std::ptrdiff_t>(keep), window.end());
		}
		else
		{
			carry.clear();
		}

		address += regionSize;
		if (address < module.base)
		{
			break; // 溢出保护（理论上不会发生）
		}
	}

	return hits;
}
} // namespace demo::scan
