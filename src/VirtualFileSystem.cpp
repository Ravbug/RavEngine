#include "VirtualFileSystem.hpp"
#include <ttvfs_zip.h>
#include <sstream>
#include <filesystem>

#ifdef __APPLE__
    #include <CoreFoundation/CFBundle.h>
#endif

using namespace RavEngine;
using namespace std;

VirtualFilesystem::VirtualFilesystem(const std::string& path) {
#ifdef __APPLE__
    CFBundleRef AppBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(AppBundle);
    CFStringRef resourcePath = CFURLCopyPath(resourcesURL);

    string bundlepath = CFStringGetCStringPtr(resourcePath, kCFStringEncodingUTF8);
    const char* cstr = (bundlepath + "/" + path).c_str();
    
    CFRelease(resourcePath);
    CFRelease(resourcesURL);
#else
    const char* cstr = path.c_str();
#endif
    
	//configure
	vfs.AddLoader(new ttvfs::DiskLoader);
	vfs.AddArchiveLoader(new ttvfs::VFSZipArchiveLoader);
	
	//mount the archive
	vfs.AddArchive(cstr);

	rootname = path + "/";
}
const std::string RavEngine::VirtualFilesystem::FileContentsAt(const std::string& path)
{
	ttvfs::File* vf = nullptr;
	vf = vfs.GetFile((rootname + path).c_str());

	if (vf == nullptr){
		throw runtime_error("cannot open " + filesystem::current_path().string() + "/" + rootname + path);
	}
	
	//try to locate and open
	if (vf && vf->open("r")) {
		const auto size = vf->size();
		char* filedata = new char[size];
		vf->read(filedata, size);
		string cpy(filedata,size);	//force all bytes
		delete[] filedata;
		return cpy;
	}
	else {
		throw runtime_error("Cannot open " + path);
	}
}

bool RavEngine::VirtualFilesystem::Exists(const std::string& path)
{
	return vfs.GetFile((rootname + path).c_str()) != nullptr;
}


RavEngine::VirtualFilesystem::~VirtualFilesystem()
{
}
