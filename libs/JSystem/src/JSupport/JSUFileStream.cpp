#include <JSystem/JSupport/JSUFileInputStream.hpp>
#include <JSystem/JSupport/JSUFileOutputStream.hpp>

JSUFileInputStream::JSUFileInputStream(JKRFile* file)
{
	mFile     = file;
	mPosition = 0;
}

// TODO: 0x4c in the map; nothing in the binary shows what open does besides
// presumably forwarding to mFile->open.
bool JSUFileInputStream::open(const char*) { return false; }

// The 0x30 of a single virtual call through mFile, the shape of getLength.
// Whether it returns the file's bool or nothing, the size cannot settle.
bool JSUFileInputStream::close() { return mFile->close(); }

int JSUFileInputStream::readData(void* buf, s32 count)
{
	int result = 0;

	if (mFile->isAvailable()) {
		if (mPosition + (u32)count > mFile->getFileSize()) {
			count = mFile->getFileSize() - mPosition;
		}

		if (count > 0) {
			result = mFile->readData(buf, count, mPosition);
			mPosition += result;
		}
	}

	return result;
}

int JSUFileInputStream::seekPos(s32 offset, JSUStreamSeekFrom from)
{

	s32 oldPosition = mPosition;

	switch (from) {
	case JSUStreamSeekFrom_SET:
		mPosition = offset;
		break;

	case JSUStreamSeekFrom_END:
		mPosition = mFile->getFileSize() - offset;
		break;

	case JSUStreamSeekFrom_CUR:
		mPosition += offset;
		break;
	}

	if (mPosition < 0)
		mPosition = 0;

	if (mPosition > mFile->getFileSize())
		mPosition = mFile->getFileSize();

	return mPosition - oldPosition;
}

JSUFileOutputStream::JSUFileOutputStream(JKRFile* file)
{
	mFile     = file;
	mPosition = 0;
}

// TODO: 0x4c in the map, as JSUFileInputStream::open.
bool JSUFileOutputStream::open(const char*) { return false; }

bool JSUFileOutputStream::close() { return mFile->close(); }

// readData without the clamp to the file size, which a write extends.
int JSUFileOutputStream::writeData(const void* buf, s32 count)
{
	int result = 0;

	if (mFile->isAvailable()) {
		if (count > 0) {
			result = mFile->writeData(buf, count, mPosition);
			mPosition += result;
		}
	}

	return result;
}

int JSUFileOutputStream::seekPos(s32 offset, JSUStreamSeekFrom from)
{
	s32 oldPosition = mPosition;

	switch (from) {
	case JSUStreamSeekFrom_SET:
		mPosition = offset;
		break;

	case JSUStreamSeekFrom_END:
		mPosition = mFile->getFileSize() - offset;
		break;

	case JSUStreamSeekFrom_CUR:
		mPosition += offset;
		break;
	}

	if (mPosition < 0)
		mPosition = 0;

	if (mPosition > mFile->getFileSize())
		mPosition = mFile->getFileSize();

	return mPosition - oldPosition;
}
