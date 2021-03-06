#pragma once
#include <string>
#include <vector>

namespace RavEngine{
class VirtualFilesystem {
public:
	VirtualFilesystem(const std::string&);

	/**
	 Get the file data as a string
	 @param path the resources path to the asset
	 @return the file data
	 */
	const std::string FileContentsAt(const char* path);
	
	/**
	 Get the file data in a vector
	 @param path the resources path to the asset
	 @param datavec the vector to write the data into
	 */
	void FileContentsAt(const char* path, std::vector<uint8_t>& datavec);

	/**
	 @return true if the VFS has the file at the path
	 */
	bool Exists(const char* path);
	
protected:
	std::string rootname;
};
}
