// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include "libaegisub/fs.h"

#include "libaegisub/access.h"
#include "libaegisub/log.h"

#include <boost/algorithm/string/predicate.hpp>
#define BOOST_NO_SCOPED_ENUMS
#include <boost/filesystem/operations.hpp>
#undef BOOST_NO_SCOPED_ENUMS

namespace bfs = boost::filesystem;
namespace ec = boost::system::errc;

// boost::filesystem functions throw a single exception type for all
// errors, which isn't really what we want, so do some crazy wrapper
// shit to map error codes to more useful exceptions.
#ifdef BOOST_WINDOWS_API
#include <winerror.h>
#define CHECKED_CALL(exp, src_path, dst_path) \
	boost::system::error_code ec; \
	exp; \
	switch (ec.value()) {\
		case ERROR_SUCCESS: break; \
		case ERROR_FILE_NOT_FOUND: throw FileNotFound(src_path); \
		case ERROR_DIRECTORY: throw NotADirectory(src_path); \
		case ERROR_DISK_FULL: throw DriveFull(dst_path); \
		case ERROR_ACCESS_DENIED: \
			if (!src_path.empty()) \
				acs::CheckFileRead(src_path); \
			if (!dst_path.empty()) \
				acs::CheckFileWrite(dst_path); \
			throw AccessDenied(src_path); \
		default: \
			LOG_D("filesystem") << "Unknown error when calling '" << #exp << "': " << ec << ": " << ec.message(); \
			throw FileSystemUnknownError(ec.message()); \
	}
#else
#define CHECKED_CALL(exp, src_path, dst_path) \
	boost::system::error_code ec; \
	exp; \
	switch (ec.value()) {\
		case ec::success: break; \
		case ec::no_such_file_or_directory: throw FileNotFound(src_path); \
		case ec::is_a_directory: throw NotAFile(src_path); \
		case ec::not_a_directory: throw NotADirectory(src_path); \
		case ec::no_space_on_device: throw DriveFull(dst_path); \
		case ec::permission_denied: \
			if (!src_path.empty()) \
				acs::CheckFileRead(src_path); \
			if (!dst_path.empty()) \
				acs::CheckFileWrite(dst_path); \
			throw AccessDenied(src_path); \
		default: \
			LOG_D("filesystem") << "Unknown error when calling '" << #exp << "': " << ec << ": " << ec.message(); \
			throw FileSystemUnknownError(ec.message()); \
	}
#endif

#define CHECKED_CALL_RETURN(exp, src_path) \
	CHECKED_CALL(auto ret = exp, src_path, agi::fs::path()); \
	return ret

#define WRAP_BFS(bfs_name, agi_name) \
	auto agi_name(path const& p) -> decltype(bfs::bfs_name(p)) { \
		CHECKED_CALL_RETURN(bfs::bfs_name(p, ec), p); \
	}

#define WRAP_BFS_IGNORE_ERROR(bfs_name, agi_name) \
	auto agi_name(path const& p) -> decltype(bfs::bfs_name(p)) { \
		boost::system::error_code ec; \
		return bfs::bfs_name(p, ec); \
	}

// sasuga windows.h
#undef CreateDirectory

namespace agi::fs {
/*namespace {
	WRAP_BFS(file_size, SizeImpl)
	WRAP_BFS(space, Space)
}

	WRAP_BFS_IGNORE_ERROR(exists, Exists)
	WRAP_BFS_IGNORE_ERROR(is_regular_file, FileExists)
	WRAP_BFS_IGNORE_ERROR(is_directory, DirectoryExists)
	WRAP_BFS(last_write_time, ModifiedTime)
	WRAP_BFS(create_directories, CreateDirectory)
	WRAP_BFS(remove, Remove)
	WRAP_BFS(canonical, Canonicalize)*/

    //std::time_t ModifiedTime(std::filesystem::path const& file_path) {
    //    std::filesystem::file_time_type last_modified = std::filesystem::last_write_time(file_path);
    //}

	uintmax_t Size(std::filesystem::path const& p) {
		if (std::filesystem::is_directory(p))
			throw NotAFile(p);
		return file_size(p);
	}

	uintmax_t FreeSpace(std::filesystem::path const& p) {
		return space(p).available;
	}

	void Rename(const std::filesystem::path& from, const std::filesystem::path& to) {
		CHECKED_CALL(std::filesystem::rename(from, to, ec), from, to);
	}

	bool HasExtension(std::filesystem::path const& p, std::string_view ext) {
		std::string_view filename = p.filename().string();
		if (filename.size() < ext.size() + 1) return false;
		if (filename[filename.size() - ext.size() - 1] != '.') return false;
		return boost::iends_with(filename, ext);
	}
}
