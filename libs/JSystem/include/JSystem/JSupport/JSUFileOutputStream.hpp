#ifndef JSU_FILE_OUTPUT_STREAM_HPP
#define JSU_FILE_OUTPUT_STREAM_HPP

#include <JSystem/JSupport/JSURandomOutputStream.hpp>
#include <JSystem/JKernel/JKRFile.hpp>

// The whole class is dead-stripped; its map sizes repeat JSUFileInputStream's
// member for member (ctor 0x44, seekPos 0xe8, dtor 0x74, getLength 0x30,
// getPosition 0x8), and its 0x28 vtable is JSURandomOutputStream's eight slots.
class JSUFileOutputStream : public JSURandomOutputStream {
public:
	JSUFileOutputStream(JKRFile* file);
	bool open(const char* name);
	bool close();
	virtual int writeData(const void* buf, s32 count);
	virtual int seekPos(s32 offset, JSUStreamSeekFrom from);
	virtual ~JSUFileOutputStream() { }
	virtual int getLength() const { return mFile->getFileSize(); }
	virtual int getPosition() const { return mPosition; }

	JKRFile* mFile;
	s32 mPosition;
};

#endif
