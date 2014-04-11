/* vim: set ai noet ts=4 sw=4 tw=115: */
//
// Copyright (c) 2014 Nikolay Zapolnov (zapolnov@gmail.com).
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "path-util.h"
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef INCLUDE_DIRENT_H
#include INCLUDE_DIRENT_H
#else
#include <yip-imports/dirent.h>
#endif

#ifndef _WIN32
 #include <unistd.h>
 #include <pwd.h>
 #include <sys/types.h>
 #ifdef __APPLE__
  #include <mach-o/dyld.h>
 #endif
 #ifdef __FreeBSD__
  #include <sys/sysctl.h>
 #endif
#else
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <shlwapi.h>
 #include <userenv.h>
#endif

std::string pathToNativeSeparators(const std::string & path)
{
  #ifndef _WIN32
	return path;
  #else
	std::string result = path;
	for (char & ch : result)
	{
		if (ch == '/')
			ch = '\\';
	}
	return result;
  #endif
}

std::string pathToUnixSeparators(const std::string & path)
{
  #ifndef _WIN32
	return path;
  #else
	std::string result = path;
	for (char & ch : result)
	{
		if (ch == '\\')
			ch = '/';
	}
	return result;
  #endif
}

const char * pathSeparator()
{
  #ifdef _WIN32
	return "\\";
  #else
	return "/";
  #endif
}

bool pathIsSeparator(char ch)
{
	if (ch == '/')
		return true;

  #ifndef _WIN32
	return false;
  #else
	return ch == '\\';
  #endif
}

bool pathIsWin32DriveLetter(char ch)
{
	return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
}

bool pathIsWin32PathWithDriveLetter(const std::string & path)
{
	return (path.length() >= 2 && path[1] == ':' && pathIsWin32DriveLetter(path[0]));
}

std::string pathGetCurrentDirectory()
{
  #ifndef _WIN32
	std::vector<char> buf(std::max(static_cast<size_t>(PATH_MAX), static_cast<size_t>(2048)));
	if (!getcwd(buf.data(), buf.size()))
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to determine current directory: " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return buf.data();
  #else
	DWORD size = GetCurrentDirectoryA(0, nullptr);
	if (size == 0)
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine current directory (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	std::vector<char> buf(size);
	DWORD len = GetCurrentDirectoryA(size, buf.data());
	if (len == 0 || len >= size)
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine current directory (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	return std::string(buf.data(), len);
  #endif
}

std::string pathGetUserHomeDirectory()
{
  #ifndef _WIN32
	const char * env = getenv("HOME");
	if (env)
		return env;

	struct passwd * pw = getpwuid(getuid());
	if (pw && pw->pw_dir && pw->pw_dir[0])
		return pw->pw_dir;

	throw std::runtime_error("unable to determine path to the user home directory.");
  #else
	std::string result;
	HANDLE hToken = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine path to the user home directory (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	try
	{
		std::vector<char> buf(MAX_PATH);
		DWORD size = MAX_PATH;
		if (!GetUserProfileDirectoryA(hToken, buf.data(), &size))
		{
			DWORD err = GetLastError();
			std::stringstream ss;
			ss << "unable to determine path to the user home directory (code " << err << ").";
			throw std::runtime_error(ss.str());
		}
		result = buf.data();
	}
	catch (...)
	{
		CloseHandle(hToken);
		throw;
	}
	CloseHandle(hToken);
	return result;
  #endif
}

bool pathIsAbsolute(const std::string & path)
{
  #ifndef _WIN32
	if (path.length() >= 1 && path[0] == '~')
		return (path.length() == 1 || pathIsSeparator(path[1]));
	return pathIsSeparator(path[0]);
  #else
	if (PathIsRelativeA(path.c_str()))
		return false;
	return (path.length() > 2 && pathIsWin32PathWithDriveLetter(path) && pathIsSeparator(path[2]));
  #endif
}

std::string pathMakeAbsolute(const std::string & path, const std::string & basePath)
{
  #ifndef _WIN32
	if (path.length() >= 1 && path[0] == '~')
	{
		if (path.length() == 1)
			return pathGetUserHomeDirectory();
		else if (pathIsSeparator(path[1]))
			return pathSimplify(pathConcat(pathGetUserHomeDirectory(), path.substr(2)));
	}
	if (pathIsSeparator(path[0]))
		return pathSimplify(path);
  #else
	if (pathIsWin32PathWithDriveLetter(path) || (path.length() > 0 && pathIsSeparator(path[0])))
		return pathMakeAbsolute(path);
  #endif

	return pathSimplify(pathConcat(basePath, path));
}

std::string pathMakeAbsolute(const std::string & path)
{
  #ifndef _WIN32
	return pathMakeAbsolute(path, pathGetCurrentDirectory());
  #else
	DWORD size = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
	if (size == 0)
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine absolute path for file '" << path << "' (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	std::vector<char> buf(size);
	DWORD len = GetFullPathNameA(path.c_str(), size, buf.data(), nullptr);
	if (len == 0 || len >= size)
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine absolute path for file '" << path << "' (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	return std::string(buf.data(), len);
  #endif
}

size_t pathIndexOfFirstSeparator(const std::string & path, size_t start)
{
	size_t pos = path.find('/', start);

  #ifdef _WIN32
	size_t pos2 = path.find('\\', start);
	if (pos2 != std::string::npos && (pos == std::string::npos || pos2 < pos))
		pos = pos2;
  #endif

	return pos;
}

std::string pathSimplify(const std::string & path)
{
	std::vector<std::string> parts;
	std::stringstream ss;
	size_t off = 0;

  #ifndef _WIN32
	if (path.length() > 0 && path[0] == '~')
	{
		if (path.length() == 1)
			return path;
		else if (pathIsSeparator(path[1]))
		{
			ss << '~' << pathSeparator();
			off = 2;
		}
	}
	else if (path.length() > 0 && pathIsSeparator(path[0]))
	{
		off = 1;
		ss << pathSeparator();
	}
  #else
	if (path.length() >= 2 && path[0] == path[1] && pathIsSeparator(path[0]))
	{
		off = pathIndexOfFirstSeparator(path, 2);
		if (off == std::string::npos)
			return pathToNativeSeparators(path);
		ss << path.substr(0, off) << pathSeparator();
		++off;
	}
	else if (pathIsWin32PathWithDriveLetter(path))
	{
		off = 2;
		ss << path.substr(0, off);
		if (path.length() > 2 && pathIsSeparator(path[2]))
		{
			++off;
			ss << pathSeparator();
		}
	}
	else if (path.length() > 0 && pathIsSeparator(path[0]))
	{
		off = 1;
		ss << pathSeparator();
	}
  #endif

	for (;;)
	{
		size_t pos = pathIndexOfFirstSeparator(path, off);
		if (pos == std::string::npos)
		{
			std::string part = path.substr(off);
			if (part.length() > 0)
				parts.push_back(part);
			break;
		}

		std::string part = path.substr(off, pos - off);
		off = pos + 1;

		if (part.length() > 0 && part != ".")
		{
			if (part == ".." && parts.size() > 0 && parts.back() != "..")
				parts.pop_back();
			else
				parts.push_back(part);
		}
	}

	const char * prefix = "";
	for (const std::string & part : parts)
	{
		ss << prefix << part;
		prefix = pathSeparator();
	}

	return ss.str();
}

std::string pathMakeCanonical(const std::string & path)
{
  #ifndef _WIN32
	std::vector<char> buf(std::max(static_cast<size_t>(PATH_MAX), static_cast<size_t>(2048)));
	if (!realpath(path.c_str(), buf.data()))
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to canonicalize path '" << path << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return buf.data();
  #else
	return pathMakeAbsolute(path);
  #endif
}

std::string pathConcat(const std::string & path1, const std::string & path2)
{
	if (path1.length() == 0)
		return path2;
	if (path2.length() == 0)
		return path1;

	if (pathIsSeparator(path1[path1.length() - 1]))
		return path1 + path2;
	else
		return path1 + pathSeparator() + path2;
}

size_t pathIndexOfFileName(const std::string & path)
{
	size_t pos = path.rfind('/');

  #ifdef _WIN32
	size_t pos2 = path.rfind('\\');
	if (pos2 != std::string::npos && (pos == std::string::npos || pos2 > pos))
		pos = pos2;
	if (pos == std::string::npos && pathIsWin32PathWithDriveLetter(path))
		pos = 1;
  #endif

	return (pos != std::string::npos ? pos + 1 : 0);
}

std::string pathGetDirectory(const std::string & path)
{
	size_t pos = pathIndexOfFileName(path);
	if (pos > 0)
		--pos;
	return path.substr(0, pos);
}

std::string pathGetFileName(const std::string & path)
{
	return path.substr(pathIndexOfFileName(path));
}

std::string pathGetShortFileExtension(const std::string & path)
{
	size_t pos = path.rfind('.');
	if (pos == std::string::npos)
		return std::string();

	size_t offset = pathIndexOfFileName(path);
	if (pos < offset)
		return std::string();

	return path.substr(pos);
}

std::string pathGetFullFileExtension(const std::string & path)
{
	size_t offset = pathIndexOfFileName(path);
	size_t pos = path.find('.', offset);
	return (pos == std::string::npos ? std::string() : path.substr(pos));
}

std::string pathReplaceFullFileExtension(const std::string & path, const std::string & ext)
{
	size_t offset = pathIndexOfFileName(path);
	size_t pos = path.find('.', offset);
	return (pos == std::string::npos ? path + ext : path.substr(0, pos) + ext);
}

bool pathCreate(const std::string & path)
{
	std::string dir = pathMakeAbsolute(path);
	bool result = false;
	size_t off = 0;

  #ifndef _WIN32
	if (pathIsSeparator(dir[0]))
		off = 1;
	else
	{
		std::stringstream ss;
		ss << "invalid path '" << dir << "'.";
		throw std::runtime_error(ss.str());
	}
  #else
	if (pathIsWin32PathWithDriveLetter(dir))
		off = 3;
	else if (PathIsUNCA(dir.c_str()))
	{
		off = pathIndexOfFirstSeparator(dir, 2);
		if (off == std::string::npos)
		{
			std::stringstream ss;
			ss << "invalid path '" << dir << "'.";
			throw std::runtime_error(ss.str());
		}
		++off;
	}
	else
	{
		std::stringstream ss;
		ss << "invalid path '" << dir << "'.";
		throw std::runtime_error(ss.str());
	}
  #endif

	size_t end;
	do
	{
		end = pathIndexOfFirstSeparator(dir, off);
		off = end + 1;

		std::string subdir;
		if (end == std::string::npos)
			subdir = dir;
		else
			subdir = dir.substr(0, end);

	  #ifndef _WIN32
		if (mkdir(subdir.c_str(), 0755) == 0)
			result = true;
		else
		{
			int err = errno;
			if (err != EEXIST)
			{
				std::stringstream ss;
				ss << "unable to create directory '" << subdir << "': " << strerror(err);
				throw std::runtime_error(ss.str());
			}
		}
	  #else
		if (CreateDirectoryA(subdir.c_str(), nullptr))
			result = true;
		else
		{
			DWORD err = GetLastError();
			if (err != ERROR_ALREADY_EXISTS)
			{
				std::stringstream ss;
				ss << "unable to create directory '" << subdir << "' (code " << err << ").";
				throw std::runtime_error(ss.str());
			}
		}
	  #endif
	}
	while (end != std::string::npos);

	return result;
}

bool pathIsExistent(const std::string & path)
{
	struct stat st;
	int err = stat(path.c_str(), &st);
	return (err == 0);
}

bool pathIsFile(const std::string & path)
{
  #ifndef _WIN32
	struct stat st;
	int err = stat(path.c_str(), &st);
	if (err < 0)
	{
		err = errno;
		if (err == ENOENT)
			return false;
		std::stringstream ss;
		ss << "unable to stat file '" << path << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return S_ISREG(st.st_mode);
  #else
	DWORD attr = GetFileAttributesA(path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		DWORD err = GetLastError();
		if (err = ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
				err == ERROR_INVALID_DRIVE || err == ERROR_BAD_NETPATH)
			return false;
		std::stringstream ss;
		ss << "unable to get attributes for file '" << path << "' (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
	return (attr & (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY)) == 0;
  #endif
}

time_t pathGetModificationTime(const std::string & path)
{
	struct stat st;
	int err = stat(path.c_str(), &st);
	if (err < 0)
	{
		err = errno;
		std::stringstream ss;
		ss << "unable to stat file '" << path << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return st.st_mtime;
}

std::string pathGetThisExecutableFile()
{
  #ifdef _WIN32
	char buf[MAX_PATH];
	if (!GetModuleFileNameA(nullptr, buf, sizeof(buf)))
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to determine file name of executable file (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
  #elif defined(__APPLE__)
	uint32_t size = 0;
	char tempChar = 0;
	if (_NSGetExecutablePath(&tempChar, &size) >= 0 || size == 0)
	{
		throw std::runtime_error(
			"unable to determine file name of executable file (_NSGetExecutablePath has weird behavior).");
	}
	std::vector<char> buf(size + 1);
	if (_NSGetExecutablePath(buf.data(), &size) < 0)
	{
		throw std::runtime_error
			("unable to determine file name of executable file (_NSGetExecutablePath has failed).");
	}
	return buf.data();
  #elif defined(__linux__) || defined(__ANDROID__)
	std::vector<char> buf(PATH_MAX + 1);
	if (readlink("/proc/self/exe", buf.data(), PATH_MAX) < 0)
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to read link '/proc/self/exe': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return buf.data();
  #elif defined(__FreeBSD__)
	int mib[4] = {
		CTL_KERN,
		KERN_PROC,
		KERN_PROC_PATHNAME,
		-1
	};
	std::vector<char> buf(PATH_MAX + 1);
	if (sysctl(mib, 4, buf, PATH_MAX, nullptr, 0) < 0)
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to determine file name of executable file: " << strerror(err);
		throw std::runtime_error(ss.str());
	}
	return buf.data();
  #elif defined(sun) || defined(__sun) || defined(SUNOS)
	const char * path = getexecname();
	if (!path || !*path)
		throw std::runtime_error("unable to determine file name of executable file.");
	return path;
  #else
	throw std::runtime_error("unable to determine file name of executable file (not implemented).");
  #endif
}

std::string pathCreateSymLink(const std::string & from, const std::string & to)
{
  #ifdef _WIN32
	if (!CreateSymbolicLinkA(to.c_str(), from.c_str(), 0))
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to create symlink from '" << from << "' to '" << to << " (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
  #else
	int r = symlink(from.c_str(), to.c_str());
	if (r < 0)
	{
		int err = errno;
		if (err == EEXIST)
		{
			std::vector<char> buf(PATH_MAX + 1);
			if (readlink(to.c_str(), buf.data(), PATH_MAX) >= 0 && from == buf.data())
				return to;
		}
		std::stringstream ss;
		ss << "unable to create symlink from '" << from << "' to '" << to << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
  #endif

	return to;
}

DirEntryList pathEnumDirectoryContents(const std::string & path)
{
	DirEntryList list;
	DIR * dir;

	dir = opendir(path.c_str());
	if (!dir)
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to enumerate contents of directory '" << path << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}

	try
	{
		struct dirent * ent;
		while ((ent = readdir(dir)) != NULL)
		{
			DirEntry entry;

			entry.name = ent->d_name;
			if (entry.name == "." || entry.name == "..")
				continue;

			switch (ent->d_type)
			{
			case DT_REG: entry.type = DirEntry_RegularFile; break;
			case DT_DIR: entry.type = DirEntry_Directory; break;
			case DT_FIFO: entry.type = DirEntry_FIFO; break;
			case DT_SOCK: entry.type = DirEntry_Socket; break;
			case DT_CHR: entry.type = DirEntry_CharDevice; break;
			case DT_BLK: entry.type = DirEntry_BlockDevice; break;
			case DT_LNK: entry.type = DirEntry_Link; break;
			default: entry.type = DirEntry_Unknown; break;
			}

			list.push_back(entry);
		}
	}
	catch (...)
	{
		closedir(dir);
		throw;
	}

	closedir(dir);

	return list;
}

void pathDeleteFile(const std::string & path)
{
  #ifndef _WIN32
	if (unlink(path.c_str()) < 0)
	{
		int err = errno;
		std::stringstream ss;
		ss << "unable to delete file '" << path << "': " << strerror(err);
		throw std::runtime_error(ss.str());
	}
  #else
	if (!DeleteFileA(path.c_str()))
	{
		DWORD err = GetLastError();
		std::stringstream ss;
		ss << "unable to delete file '" << path << "' (code " << err << ").";
		throw std::runtime_error(ss.str());
	}
  #endif
}
