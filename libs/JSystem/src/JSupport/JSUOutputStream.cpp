#include <JSystem/JSupport/JSUOutputStream.hpp>
#include <JSystem/JSupport/JSURandomOutputStream.hpp>

JSUOutputStream::~JSUOutputStream() { }

int JSUOutputStream::write(const void* buf, s32 size)
{
	int len = writeData(buf, size);
	if (len != size) {
		setState(EIoState_EOF);
	}
	return len;
}

// TODO: write__15JSUOutputStreamFPCc (UNUSED, 0xfc) belongs here. The mirror
// of JSUInputStream::read(char*) (a u16 strlen, then the bytes) is only 0xac.
int JSUOutputStream::skip(s32 amount, s8 val)
{
	int i;
	for (i = 0; i < amount; ++i) {
		u32 r = writeData(&val, 1);
		if (r != 1) {
			setState(EIoState_EOF);
			break;
		}
	}
	return i;
}

// TODO: align__21JSURandomOutputStreamFlSc (UNUSED, 0x80) belongs here.
// JSURandomInputStream::align with the gap written by the virtual skip is 0x9c,
// and 0x8c without the EOF test; the body is not settled.

// UNUSED. JSURandomInputStream::peek with writeData (both 0xbc).
u32 JSURandomOutputStream::poke(void* buf, s32 len)
{
	int r   = 0;
	int pos = getPosition();
	r       = writeData(buf, len);
	if (r != len) {
		setState(EIoState_EOF);
	}
	if (r != 0) {
		seekPos(pos, JSUStreamSeekFrom_SET);
	}

	return r;
}

void JSURandomOutputStream::seek(s32 offset, JSUStreamSeekFrom from)
{
	seekPos(offset, from);
	clrState(EIoState_EOF);
}
