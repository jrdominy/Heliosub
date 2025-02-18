// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
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

/// @file charset.cpp
/// @brief Character set detection and manipulation utilities.
/// @ingroup libaegisub

#include "libaegisub/charset.h"

#include "libaegisub/file_mapping.h"
#include "libaegisub/scoped_ptr.h"
#include <filesystem>

#ifdef WITH_UCHARDET
#include <uchardet/uchardet.h>
#include <boost/algorithm/string/case_conv.hpp>
#endif

namespace agi::charset {
std::string Detect(std::filesystem::path const& file) {
	agi::read_file_mapping fp(file);

	// FIXME: It is an empty file. Treat as ascii
	if (fp.size() == 0) return "ascii";

	// First check for known magic bytes which identify the file type
	if (fp.size() >= 4) {
		const char* header = fp.read(0, 4);
		if (!strncmp(header, "\xef\xbb\xbf", 3))
			return "utf-8";
		if (!strncmp(header, "\x00\x00\xfe\xff", 4))
			return "utf-32be";
		if (!strncmp(header, "\xff\xfe\x00\x00", 4))
			return "utf-32le";
		if (!strncmp(header, "\xfe\xff", 2))
			return "utf-16be";
		if (!strncmp(header, "\xff\xfe", 2))
			return "utf-16le";
		if (!strncmp(header, "\x1a\x45\xdf\xa3", 4))
			return "binary"; // Actually EBML/Matroska
	}

#ifdef WITH_UCHARDET
	agi::scoped_holder<uchardet_t> ud(uchardet_new(), uchardet_delete);
	for (uint64_t offset = 0; offset < fp.size(); ) {
		auto read = std::min<uint64_t>(65536, fp.size() - offset);
		auto buf = fp.read(offset, read);
		uchardet_handle_data(ud, buf, read);
		offset += read;
	}
	uchardet_data_end(ud);
	std::string encoding = uchardet_get_charset(ud);

	// uchardet does not tell us the byte order of UTF-16 / UTF-32, so do it ourself
	std::string encoding_lower{ encoding };
	boost::to_lower(encoding_lower);
	if (encoding_lower == "utf-16") {
		uint64_t le_score = 0, be_score = 0;
		for (uint64_t offset = 0; offset < fp.size(); ) {
			auto read = std::min<uint64_t>(65536, fp.size() - offset);
			auto buf = fp.read(offset, read);
			for (uint64_t i = 0; i + 1 < read; i += 2) {
				if (!buf[i])
					++be_score;
				if (!buf[i + 1])
					++le_score;
			}
			offset += read;
		}
		return le_score < be_score ? "utf-16be" : "utf-16le";
	}
	else if (encoding_lower == "utf-32") {
		uint64_t le_score = 0, be_score = 0;
		for (uint64_t offset = 0; offset < fp.size(); ) {
			auto read = std::min<uint64_t>(65536, fp.size() - offset);
			auto buf = fp.read(offset, read);
			for (uint64_t i = 0; i + 3 < read; i += 2) {
				if (!buf[i])
					++be_score;
				if (!buf[i + 3])
					++le_score;
			}
			offset += read;
		}
		return le_score < be_score ? "utf-32be" : "utf-32le";
	}
	return encoding.empty() ? "binary" : encoding;
#else
	// If it's over 100 MB it's either binary or big enough that we won't
	// be able to do anything useful with it anyway
	if (fp.size() > 100 * 1024 * 1024)
		return "binary";

	uint64_t binaryish = 0;
	auto read = std::min<uint64_t>(65536, fp.size());
	auto buf = fp.read(0, read);
	for (size_t i = 0; i < read; ++i) {
		if ((unsigned char)buf[i] < 32 && (buf[i] != '\r' && buf[i] != '\n' && buf[i] != '\t'))
			++binaryish;
	}

	if (binaryish > read / 8)
		return "binary";
	return "utf-8";
#endif
}
}
