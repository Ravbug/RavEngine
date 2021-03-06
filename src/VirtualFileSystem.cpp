#include "VirtualFileSystem.hpp"
#include <physfs.h>
#include <filesystem>
#include "Debug.hpp"
#include <fmt/format.h>

#ifdef __APPLE__
    #include <CoreFoundation/CFBundle.h>
#endif

using namespace RavEngine;
using namespace std;

inline const char* PHYSFS_WHY(){
	return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
}

VirtualFilesystem::VirtualFilesystem(const std::string& path) {
#ifdef __APPLE__
    CFBundleRef AppBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(AppBundle);
	CFURLRef absoluteResourceURL = CFURLCopyAbsoluteURL(resourcesURL);
	CFStringRef resourcePath = CFURLCopyPath( absoluteResourceURL);

    string bundlepath = CFStringGetCStringPtr(resourcePath, kCFStringEncodingUTF8);
	bundlepath = (bundlepath + path);
    const char* cstr = bundlepath.c_str();
    
	CFRelease(absoluteResourceURL);
    CFRelease(resourcePath);
    CFRelease(resourcesURL);
#else
    const char* cstr = path.c_str();
#endif

	//1 means add to end, can put 0 to make it first searched
	PHYSFS_mount(cstr, "", 1);
	auto root = PHYSFS_enumerateFiles("/");
	if (*root == NULL) {
		Debug::Fatal("PHYSFS Error: {}", PHYSFS_WHY());
	}
	rootname = std::filesystem::path(path).replace_extension("").string();
}
const std::string RavEngine::VirtualFilesystem::FileContentsAt(const char* path)
{
	
	auto fullpath = StrFormat("{}/{}",rootname,path);
	
	if(!Exists(path)){
		Debug::Fatal("cannot open {}{}",rootname,path);
	}
	
	auto ptr = PHYSFS_openRead(fullpath.c_str());
	auto size = PHYSFS_fileLength(ptr)+1;
	
	char* buffer = new char[size];
	
	size_t length_read = PHYSFS_readBytes(ptr,buffer,size);
	buffer[size-1] = '\0';	//add null terminator
	PHYSFS_close(ptr);
	
	//TODO: remove extra copy, perhaps use string_view?
	string cpy(buffer,size);
	delete[] buffer;
	
	return cpy;
}

void RavEngine::VirtualFilesystem::FileContentsAt(const char* path, std::vector<uint8_t>& datavec)
{
	
	string fullpath = StrFormat("{}/{}",rootname,path);
	
	if(!Exists(path)){
		Debug::Fatal("cannot open {}{}",rootname,path);
	}
	
	auto ptr = PHYSFS_openRead(fullpath.c_str());
	auto size = PHYSFS_fileLength(ptr);
	
	datavec.resize(size);
	
	//this version of the call does not need to add a null terminator
	size_t length_read = PHYSFS_readBytes(ptr,&datavec[0],size);
	PHYSFS_close(ptr);
}

bool RavEngine::VirtualFilesystem::Exists(const char* path)
{
	return PHYSFS_exists(StrFormat("{}/{}",rootname,path).c_str());
}
