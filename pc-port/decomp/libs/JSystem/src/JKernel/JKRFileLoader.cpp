#include <dolphin/os.h>
#include <stdio.h>
#include <string.h>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include <ctype.h>
#include <macros.h>

JSUList<JKRFileLoader> JKRFileLoader::sVolumeList;
JKRFileLoader* JKRFileLoader::sCurrentVolume;

JKRFileLoader::JKRFileLoader()
    : JKRDisposer()
    , mFileLoaderLink(this)
{
	mVolumeName = nullptr;
	mVolumeType = 0;
	mMountCount = 0;
}

JKRFileLoader::~JKRFileLoader()
{
	if (sCurrentVolume == this)
		sCurrentVolume = nullptr;
}

void JKRFileLoader::unmount()
{
	if (mMountCount != 0) {
		if (--mMountCount == 0)
			delete this;
	}
}

void JKRFileLoader::unmountAll() { JUT_ASSERT_F(false, "UNIMPLEMENTED"); }

JKRFileLoader* JKRFileLoader::getVolume(const char* name)
{
	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		if (strcmp(name, it->mVolumeName) == 0)
			return it.getObject();
	}

	return nullptr;
}

void JKRFileLoader::changeDirectory(const char* dir)
{
	JKRFileLoader* vol = findVolume(&dir);
	if (vol)
		vol->becomeCurrent(dir);
}

void* JKRFileLoader::getGlbResource(const char* path)
{
#if defined(TARGET_PC) && defined(VERSION_GMSP01)
	// PC port, PAL: the game code comes from the USA decomp. PAL keeps some
	// resources per language: /common/2d/x lives in the language archive
	// mounted as /cmn2d/x, and other files exist as name_<lang>.ext. Try the
	// path as asked, then those two PAL spellings.
	static const char* langs[] = { "en", "ge", "fr", "sp", "it" };
	u8 lang = OSGetLanguage();
	if (lang > 4)
		lang = 0;
	char alt[2][160];
	const char* tries[3] = { path, nullptr, nullptr };
	if (strncmp(path, "/common/2d/", 11) == 0) {
		snprintf(alt[0], sizeof(alt[0]), "/cmn2d/%s", path + 11);
		tries[1] = alt[0];
	}
	const char* dot = strrchr(path, '.');
	if (dot && (size_t)(dot - path) < sizeof(alt[1]) - 8) {
		snprintf(alt[1], sizeof(alt[1]), "%.*s_%s%s", (int)(dot - path), path,
		         langs[lang], dot);
		tries[2] = alt[1];
	}
	for (int i = 0; i < 3; i++) {
		if (!tries[i])
			continue;
		const char* q = tries[i];
		JKRFileLoader* loader = findVolume(&q);
		void* res = (loader == nullptr) ? nullptr : loader->getResource(q);
		if (res)
			return res;
	}
	return nullptr;
#else
	JKRFileLoader* loader = findVolume(&path);
	return (loader == nullptr) ? nullptr : loader->getResource(path);
#endif
}

void* JKRFileLoader::getGlbResource(const char* name, JKRFileLoader* fileLoader)
{
	void* resource = nullptr;
	if (fileLoader) {
		return fileLoader->getResource(0, name);
	}

	JSUList<JKRFileLoader>& volumeList = getVolumeList();
	for (JSUListIterator<JKRFileLoader> it = volumeList.getFirst();
	     it != volumeList.getEnd(); ++it) {
		resource = it->getResource(0, name);
		if (resource)
			break;
	}
	return resource;
}

size_t JKRFileLoader::readGlbResource(void* resourceBuffer, u32 bufferSize,
                                      const char* path)
{
	JUT_ASSERT_F(false, "UNIMPLEMENTED");
	return 0;
}

size_t JKRFileLoader::readGlbResource(void* resourceBuffer, u32 bufferSize,
                                      const char* name,
                                      JKRFileLoader* fileLoader)
{
	JUT_ASSERT_F(false, "UNIMPLEMENTED");
	return 0;
}

bool JKRFileLoader::removeResource(void* resource, JKRFileLoader* fileLoader)
{
	if (fileLoader) {
		return fileLoader->removeResource(resource);
	}

	JSUList<JKRFileLoader>& volumeList = getVolumeList();
	JSUListIterator<JKRFileLoader> iterator;
	for (iterator = volumeList.getFirst(); iterator != volumeList.getEnd();
	     ++iterator) {
		if (iterator->removeResource(resource)) {
			return true;
		}
	}

	return false;
}

bool JKRFileLoader::detachResource(void* resource, JKRFileLoader* fileLoader)
{
	if (fileLoader) {
		return fileLoader->detachResource(resource);
	}

	JSUList<JKRFileLoader>& volumeList = getVolumeList();
	JSUListIterator<JKRFileLoader> iterator;
	for (iterator = volumeList.getFirst(); iterator != volumeList.getEnd();
	     ++iterator) {
		if (iterator->detachResource(resource)) {
			return true;
		}
	}

	return false;
}

long JKRFileLoader::getResSize(void* resourceBuffer, JKRFileLoader* fileLoader)
{
	long ret = -1; // TODO: this feels wrong, but it matches, so whatever?

	if (fileLoader != nullptr)
		return fileLoader->getResSize(resourceBuffer);

	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		ret = it.getObject()->getResSize(resourceBuffer);
		if (ret >= 0)
			break;
	}

	return ret;
}

u32 JKRFileLoader::countFileGlb(const char* path)
{
	JUT_ASSERT_F(false, "UNIMPLEMENTED");
	return 0;
}

JKRFileLoader* JKRFileLoader::findVolume(const char** volumeName)
{
	if (*volumeName[0] != '/') {
		return sCurrentVolume;
	}

	char volumeNameBuffer[0x101];
	*volumeName = fetchVolumeName(volumeNameBuffer,
	                              ARRAY_COUNT(volumeNameBuffer), *volumeName);

	for (JSUListIterator<JKRFileLoader> it = sVolumeList.getFirst();
	     it != sVolumeList.getEnd(); ++it) {
		if (strcmp(volumeNameBuffer, it->mVolumeName) == 0)
			return it.getObject();
	}
	return nullptr;
}

JKRFileFinder* JKRFileLoader::findFirstFile(const char* volumeName)
{
	JKRFileFinder* ret = nullptr;

	JKRFileLoader* vol = findVolume(&volumeName);
	if (vol)
		ret = vol->getFirstFile(volumeName);

	return ret;
}

const char* JKRFileLoader::fetchVolumeName(char* buffer, s32 bufferSize,
                                           const char* path)
{
	static char rootPath[] = "/";
	if (strcmp(path, "/") == 0) {
		strcpy(buffer, rootPath);
		return rootPath;
	} else {
		path++;
		while (*path != 0 && *path != '/') {
			if (1 < bufferSize) {
				*buffer = _tolower(*path);
				buffer++;
				bufferSize--;
			}
			path++;
		}
		buffer[0] = '\0';
		if (path[0] == '\0')
			path = rootPath;
	}

	return path;
}
