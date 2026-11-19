#include "Patch.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace demo::patch
{
namespace
{
bool Compared(const Site& site, std::size_t index)
{
	return site.mask.empty() || site.mask[index] != 0;
}
} // namespace

bool Verify(const Site& site, std::string& why)
{
	if (site.address == 0)
	{
		why = "site address is null";
		return false;
	}
	if (site.expected.empty())
	{
		why = "expected bytes are empty";
		return false;
	}
	if (site.expected.size() != site.replacement.size())
	{
		why = "expected(" + std::to_string(site.expected.size()) + ") and replacement(" +
			std::to_string(site.replacement.size()) + ") differ in length";
		return false;
	}
	if (!site.mask.empty() && site.mask.size() != site.expected.size())
	{
		why = "mask length differs from expected";
		return false;
	}

	const auto* at = reinterpret_cast<const std::uint8_t*>(site.address);
	for (std::size_t i = 0; i < site.expected.size(); ++i)
	{
		if (!Compared(site, i))
		{
			continue;
		}

		if (at[i] != site.expected[i])
		{
			char detail[128]{};
			std::snprintf(detail, sizeof detail, "byte %zu differs: expected 0x%02X, actual 0x%02X",
				i, site.expected[i], at[i]);
			why = detail;
			return false;
		}
	}

	return true;
}

Result Apply(const Site& site)
{
	std::string why;
	if (!Verify(site, why))
	{
		return { false, why };
	}

	const auto address = reinterpret_cast<void*>(site.address);
	const std::size_t length = site.replacement.size();

	DWORD oldProtect = 0;
	if (!::VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		return { false, "VirtualProtect failed, GetLastError=" + std::to_string(::GetLastError()) };
	}

	std::memcpy(address, site.replacement.data(), length);

	DWORD ignored = 0;
	::VirtualProtect(address, length, oldProtect, &ignored);
	::FlushInstructionCache(::GetCurrentProcess(), address, length);

	return { true, {} };
}
} // namespace demo::patch
