#include <MarioUtil/ToolData.hpp>
#include <macros.h>
#include <string.h>

namespace Koga {
ToolData::ToolData() { mData = nullptr; }
ToolData::~ToolData() { }

BOOL ToolData::Attach(const void* jmapData)
{
	if (!jmapData)
		return FALSE;
	this->mData = (JMapData*)jmapData;
	return TRUE;
}

void ToolData::Detach() { mData = nullptr; }

// UNUSED Hash (0x54) and SearchItemInfo (0x8c) are the hash and item search
// the scored GetValues expand; called through them the GetValues stay exact.
u32 ToolData::Hash(const char* key)
{
	u32 stringHash = 0;
	char current_char;

	while ((current_char = *key) != 0) {
		key++;
		stringHash = (current_char + (stringHash << 8)) % 0x1FFFFD9;
	}
	return stringHash;
}

// The UNUSED u32 (0xe0) and bool (0xe4) reads apply the item's mask, the u32
// one also its shift; the s32, float and string reads take the field whole.
BOOL ToolData::GetValue(int entryIndex, const char* key, u32& pValueOut) const
{
	s32 itemIndex = SearchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}

BOOL ToolData::GetValue(int entryIndex, const char* key, s32& pValueOut) const
{
	s32 itemIndex = SearchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}

BOOL ToolData::GetValue(int entryIndex, const char* key, bool& pValueOut) const
{
	s32 itemIndex = SearchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}

BOOL ToolData::GetValue(int entryIndex, const char* key,
                        const char*& pValueOut) const
{
	s32 itemIndex = SearchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}
BOOL ToolData::GetValue(int entryIndex, const char* key, f32& pValueOut) const
{
	s32 itemIndex = SearchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}
s32 ToolData::SearchItemInfo(const char* key) const
{
	s32 nFields = mData->mNumFields;
	u32 hash    = Hash(key);

	for (int i = 0; i < nFields; ++i) {
		if (hash == mData->mItems[i].mHash) {
			return i;
		}
	}

	return -1;
}
// The UNUSED FindElements return the first entry from `start` whose field
// equals `value`; the parameter order is not visible in the sizes.
int ToolData::FindElement(int itemIndex, u32 value, int start) const
{
	for (int i = start; i < mData->mNumEntries; ++i) {
		u32 v;
		getValue(i, itemIndex, &v);
		if (v == value)
			return i;
	}
	return -1;
}

int ToolData::FindElement(int itemIndex, s32 value, int start) const
{
	for (int i = start; i < mData->mNumEntries; ++i) {
		s32 v;
		getValue(i, itemIndex, &v);
		if (v == value)
			return i;
	}
	return -1;
}

int ToolData::FindElement(int itemIndex, bool value, int start) const
{
	for (int i = start; i < mData->mNumEntries; ++i) {
		bool v;
		getValue(i, itemIndex, &v);
		if (v == value)
			return i;
	}
	return -1;
}

int ToolData::FindElement(int itemIndex, const char* value, int start) const
{
	for (int i = start; i < mData->mNumEntries; ++i) {
		const char* v;
		getValue(i, itemIndex, &v);
		if (strcmp(v, value) == 0)
			return i;
	}
	return -1;
}

// TODO: 0x5c against the map's 0x80. The other four overloads reach their
// map sizes with this loop; the float compare is nine instructions longer in
// retail and uses no pool constant (the map lists none for this TU).
int ToolData::FindElement(int itemIndex, f32 value, int start) const
{
	for (int i = start; i < mData->mNumEntries; ++i) {
		f32 v;
		getValue(i, itemIndex, &v);
		if (v == value)
			return i;
	}
	return -1;
}

} // namespace Koga
