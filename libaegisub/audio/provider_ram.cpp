// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "libaegisub/audio/provider.h"

#include <array>
#include <boost/container/stable_vector.hpp>
#include <thread>
#include <memory>

namespace {
using namespace agi;

#define CacheBits 22
#define CacheBlockSize (1 << CacheBits)

class RAMAudioProvider final : public AudioProviderWrapper {
#ifdef _MSC_VER
	boost::container::stable_vector<char[CacheBlockSize]> blockcache;
#else
	boost::container::stable_vector<std::array<char, CacheBlockSize>> blockcache;
#endif
	std::atomic<bool> cancelled = {false};
	std::thread decoder;

	void FillBuffer(void *buf, int64_t start, int64_t count) const override;

public:
	RAMAudioProvider(std::unique_ptr<AudioProvider> src)
	: AudioProviderWrapper(std::move(src))
	{
		decoded_samples = 0;

		try {
			blockcache.resize((num_samples * bytes_per_sample * channels + CacheBlockSize - 1) >> CacheBits);
		}
		catch (std::bad_alloc const&) {
			throw AudioProviderError("Not enough memory available to cache in RAM");
		}

		decoder = std::thread([&] {
			int64_t readsize = CacheBlockSize / bytes_per_sample / channels;
			for (size_t i = 0; i < blockcache.size(); i++) {
				if (cancelled) break;
				auto actual_read = std::min<int64_t>(readsize, num_samples - i * readsize);
				source->GetAudio(&blockcache[i][0], i * readsize, actual_read);
				decoded_samples += actual_read;
			}
		});
	}

	~RAMAudioProvider() {
		cancelled = true;
		decoder.join();
	}
};

void RAMAudioProvider::FillBuffer(void *buf, int64_t start, int64_t count) const {
	auto charbuf = static_cast<char *>(buf);
	for (int64_t bytes_remaining = count * bytes_per_sample * channels; bytes_remaining; ) {
		if (start >= decoded_samples) {
			memset(charbuf, 0, bytes_remaining);
			break;
		}

		const int64_t samples_per_block = CacheBlockSize / bytes_per_sample / channels;

		const size_t i = start / samples_per_block;
		const int start_offset = (start % samples_per_block) * bytes_per_sample * channels;
		const int read_size = std::min<int>(bytes_remaining, samples_per_block * bytes_per_sample * channels - start_offset);

		memcpy(charbuf, &blockcache[i][start_offset], read_size);
		charbuf += read_size;
		bytes_remaining -= read_size;
		start += read_size / bytes_per_sample / channels;
	}
}
}

namespace agi {
std::unique_ptr<AudioProvider> CreateRAMAudioProvider(std::unique_ptr<AudioProvider> src) {
	return std::make_unique<RAMAudioProvider>(std::move(src));
}
}
