#include "stdafx.h"
#include "File.hpp"
#include "Log.hpp"
#include "Buffer.hpp"

#ifdef _WIN32
/*
	Windows implementation
*/
class File_Impl
{
public:
	File_Impl(HANDLE h) : handle(h) {};
	~File_Impl()
	{
		CloseHandle(handle);
	}
	HANDLE handle;
};

File::File()
{
}
File::~File()
{
	Close();
}
bool File::OpenRead(const String& path)
{
	Close();
	HANDLE h = CreateFileA(*path,
		GENERIC_READ, // Desired Access
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		0, 0);
	if(h == INVALID_HANDLE_VALUE)
	{
		Logf("Failed to open file for reading %s: %s", Logger::Warning, *path, Utility::WindowsFormatMessage(GetLastError()));
		return false;
	}

	m_impl = new File_Impl(h);
	return true;
}
bool File::OpenWrite(const String& path, bool append /*= false*/)
{
	Close();
	HANDLE h = CreateFileA(*path,
		GENERIC_WRITE, // Desired Access
		FILE_SHARE_READ,
		nullptr,
		append ? OPEN_EXISTING : CREATE_ALWAYS,
		0, 0);
	if(h == INVALID_HANDLE_VALUE)
	{
		Logf("Failed to open file for writing %s: %s", Logger::Warning, *path, Utility::WindowsFormatMessage(GetLastError()));
		return false;
	}
	m_impl = new File_Impl(h);

	// Seek to end if append is enabled
	if(append)
		SeekReverse(0);

	return true;
}
void File::Close()
{
	if(m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}
void File::Seek(size_t pos)
{
	assert(m_impl);
	LARGE_INTEGER newPos;
	LARGE_INTEGER tpos = { 0 };
	tpos.QuadPart = pos;
	BOOL ok = SetFilePointerEx(m_impl->handle, tpos, &newPos, 0);
	assert(ok && newPos.QuadPart == pos);
}
void File::Skip(int64 amount)
{
	assert(m_impl);
	LARGE_INTEGER newPos;
	LARGE_INTEGER tpos = { 0 };
	tpos.QuadPart = amount;
	BOOL ok = SetFilePointerEx(m_impl->handle, tpos, &newPos, 1);
	assert(ok);
}
void File::SeekReverse(size_t pos)
{
	assert(m_impl);
	LARGE_INTEGER newPos;
	LARGE_INTEGER tpos = { 0 };
	tpos.QuadPart = GetSize() - pos;
	BOOL ok = SetFilePointerEx(m_impl->handle, tpos, &newPos, 2);
	assert(ok);
}
size_t File::Tell() const
{
	assert(m_impl);
	LARGE_INTEGER pos;
	LARGE_INTEGER move = { 0 };
	SetFilePointerEx(m_impl->handle, move, &pos, 1);
	return (size_t)pos.QuadPart;
}
size_t File::GetSize() const
{
	assert(m_impl);
	LARGE_INTEGER size;
	GetFileSizeEx(m_impl->handle, &size);
	return (size_t)size.QuadPart;
}
size_t File::Read(void* data, size_t len)
{
	assert(m_impl);
	size_t actual = 0;
	ReadFile(m_impl->handle, data, (DWORD)len, (DWORD*)&actual, 0);
	return actual;
}
size_t File::Write(const void* data, size_t len)
{
	assert(m_impl);
	size_t actual = 0;
	WriteFile(m_impl->handle, data, (DWORD)len, (DWORD*)&actual, 0);
	return actual;
}

uint64 File::GetLastWriteTime() const
{
	assert(m_impl);
	FILETIME ftCreate, ftAccess, ftWrite;
	if(!GetFileTime(m_impl->handle, &ftCreate, &ftAccess, &ftWrite))
		return -1;
	return (uint64&)ftWrite;
}

uint64 File::GetLastWriteTime(const String& path)
{
	HANDLE h = CreateFileA(*path, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_ALWAYS, 0, 0);
	if(h == INVALID_HANDLE_VALUE)
		return -1;

	FILETIME ftCreate, ftAccess, ftWrite;
	GetFileTime(h, &ftCreate, &ftAccess, &ftWrite);
	CloseHandle(h);
	return (uint64&)ftWrite;
}

bool LoadResourceInternal(const char* name, const char* type, Buffer& out)
{
	HMODULE module = GetModuleHandle(nullptr);
	HRSRC res = FindResourceA(module, name, type);
	if(res == INVALID_HANDLE_VALUE)
		return false;
	HGLOBAL data = ::LoadResource(module, res);
	if(!data)
		return false;

	size_t size = SizeofResource(module, res);
	void* srcPtr = LockResource(data);
	out.resize(size);
	memcpy(out.data(), srcPtr, size);
	UnlockResource(data);

	return true;
}
bool EmbeddedResource::LoadResource(const ::String& resourceName, Buffer& out, ResourceType resourceType)
{
	return LoadResourceInternal(*resourceName, MAKEINTRESOURCEA(resourceType), out);
}
bool EmbeddedResource::LoadResource(uint32 resourceID, Buffer& out, ResourceType resourceType /*= RCData*/)
{
	return LoadResourceInternal(MAKEINTRESOURCEA(resourceID), MAKEINTRESOURCEA(resourceType), out);
}
#else
/*
	Linux implementation
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// for fstat
#include <sys/types.h>
#include <sys/stat.h>

class File_Impl
{
public:
	File_Impl(int h) : handle(h) {};
	~File_Impl()
	{
		close(handle);
	}
	int handle;
};

File::File()
{
}
File::~File()
{
	Close();
}
bool File::OpenRead(const String& path)
{
	Close();
	
	int handle = open(*path, O_RDONLY);
	if(handle == -1)
	{
		Logf("Failed to open file for reading %s: %d", Logger::Warning, *path, errno);
		return false;
	}
	
	m_impl = new File_Impl(handle);
	
	return true;
}
bool File::OpenWrite(const String& path, bool append /*= false*/)
{
	Close();
	
	int flags = O_WRONLY | O_CREAT;
	if(append)
		flags |= O_APPEND;
	int handle = open(*path, flags);
	if(handle == -1)
	{
		Logf("Failed to open file for writing %s: %d", Logger::Warning, *path, errno);
		return false;
	}
	
	m_impl = new File_Impl(handle);

	return true;
}
void File::Close()
{
	if(m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}
void File::Seek(size_t pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_SET);
}
void File::Skip(int64 pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_CUR);
}
void File::SeekReverse(size_t pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_END);
}
size_t File::Tell() const
{
	assert(m_impl);
	return lseek(m_impl->handle, 0, SEEK_CUR);
}
size_t File::GetSize() const
{
	assert(m_impl);
	struct stat sb;
	fstat(m_impl->handle, &sb);
	return sb.st_size;
}
size_t File::Read(void* data, size_t len)
{
	assert(m_impl);
	return read(m_impl->handle, data, (uint32)len);
}
size_t File::Write(const void* data, size_t len)
{
	assert(m_impl);
	return write(m_impl->handle, data, (uint32)len);
}

uint64 File::GetLastWriteTime() const
{
	assert(m_impl);
	struct stat sb;
	fstat(m_impl->handle, &sb);
	return sb.st_mtim.tv_sec * (uint64)1000000000L + sb.st_mtim.tv_nsec;
}

uint64 File::GetLastWriteTime(const String& path)
{
	struct stat sb;
	if(stat(*path, &sb) != 0)
		return 0;
	return sb.st_mtim.tv_sec * (uint64)1000000000L + sb.st_mtim.tv_nsec;
}

bool LoadResourceInternal(const char* name, const char* type, Buffer& out)
{
	return false;
}
bool EmbeddedResource::LoadResource(const ::String& resourceName, Buffer& out, ResourceType resourceType)
{
	return false;
}
bool EmbeddedResource::LoadResource(uint32 resourceID, Buffer& out, ResourceType resourceType /*= RCData*/)
{
	return false;
}
#endif