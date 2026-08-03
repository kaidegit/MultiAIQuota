#include "www_store.hpp"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

// ROM miniz (tinfl): inflates the embedded gzip archive. All buffers are
// heap-allocated (the 32KB-dict style APIs are stack hungry and avoided).
#include "miniz.h"

extern "C" {
// Embedded via `EMBED_FILES` in the main component (www.tar.gz).
extern const uint8_t _binary_www_tar_gz_start[];
extern const uint8_t _binary_www_tar_gz_end[];
}

namespace hw {

namespace {

constexpr char TAG[] = "www_store";
// Upper bound for the decompressed www payload (tar, currently ~62KB).
constexpr size_t kMaxDecompressedSize = 512 * 1024;

struct WwwEntry {
    std::string name;   // path inside the archive, e.g. "assets/index-x.js"
    const uint8_t* data;
    size_t size;
};

uint8_t* s_www_data = nullptr; // decompressed tar buffer (PSRAM)
std::vector<WwwEntry> s_entries;
std::mutex s_mutex;

// Locate the raw deflate stream inside a gzip file, skipping the 10-byte
// header and any optional fields; returns the stream length (excluding the
// 8-byte CRC32+ISIZE trailer).
const uint8_t* gzip_data_start(const uint8_t* data, size_t size, size_t* deflate_size) {
    if (size < 18 || data[0] != 0x1F || data[1] != 0x8B || data[2] != 0x08) {
        return nullptr;
    }
    size_t off = 10;
    const uint8_t flg = data[3];
    if (flg & 0x04) { // FEXTRA
        if (off + 2 > size) return nullptr;
        const size_t xlen = data[off] | (size_t(data[off + 1]) << 8);
        off += 2 + xlen;
    }
    if (flg & 0x08) { // FNAME
        while (off < size && data[off] != 0) ++off;
        ++off;
    }
    if (flg & 0x10) { // FCOMMENT
        while (off < size && data[off] != 0) ++off;
        ++off;
    }
    if (flg & 0x02) off += 2; // FHCRC
    if (off >= size || size - off < 8) return nullptr;
    *deflate_size = size - off - 8;
    return data + off;
}

// Reject names escaping the archive root.
bool tar_safe_path(const char* name) {
    if (!name[0] || name[0] == '/') return false;
    for (const char* p = name; *p;) {
        const char* slash = std::strchr(p, '/');
        const size_t len = slash ? static_cast<size_t>(slash - p) : std::strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') return false;
        p = slash ? slash + 1 : p + len;
    }
    return true;
}

bool index_tar(const uint8_t* tar, size_t size) {
    size_t off = 0;
    while (off + 512 <= size) {
        const uint8_t* h = tar + off;
        bool all_zero = true;
        for (int i = 0; i < 512; ++i) {
            if (h[i]) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) break; // end of archive

        char name[101];
        std::memcpy(name, h, 100);
        name[100] = '\0';
        const size_t name_len = ::strnlen(name, 100);
        const char typeflag = static_cast<char>(h[156]);

        unsigned long size_oct = 0;
        for (int i = 124; i < 136; ++i) {
            if (h[i] == 0 || h[i] == ' ') break;
            size_oct = size_oct * 8 + static_cast<unsigned long>(h[i] - '0');
        }
        const size_t file_size = static_cast<size_t>(size_oct);

        off += 512;
        if (off + file_size > size) return false;

        if ((typeflag == '0' || typeflag == 0) && name_len > 0 && tar_safe_path(name)) {
            WwwEntry entry;
            entry.name.assign(name, name_len);
            entry.data = tar + off;
            entry.size = file_size;
            s_entries.push_back(std::move(entry));
        }
        off += (file_size + 511) & ~size_t(511);
    }
    return !s_entries.empty();
}

} // namespace

bool www_store_init() {
    std::lock_guard<std::mutex> lock(s_mutex);

    const size_t gz_size = static_cast<size_t>(_binary_www_tar_gz_end - _binary_www_tar_gz_start);
    if (gz_size == 0) {
        ESP_LOGE(TAG, "embedded www archive is empty");
        return false;
    }

    size_t deflate_size = 0;
    const uint8_t* deflate = gzip_data_start(_binary_www_tar_gz_start, gz_size, &deflate_size);
    if (!deflate) {
        ESP_LOGE(TAG, "embedded archive is not gzip");
        return false;
    }

    uint8_t* tar = static_cast<uint8_t*>(heap_caps_malloc(kMaxDecompressedSize, MALLOC_CAP_SPIRAM));
    if (!tar) {
        ESP_LOGE(TAG, "failed to allocate %zu bytes in PSRAM", kMaxDecompressedSize);
        return false;
    }

    // tinfl_decompressor is ~10KB: never allocate it on the stack (the main
    // task stack is only a few KB). Use the low-level coroutine API with a
    // heap-allocated decompressor instead of tinfl_decompress_mem_to_mem().
    auto* decomp =
        static_cast<tinfl_decompressor*>(heap_caps_malloc(sizeof(tinfl_decompressor), MALLOC_CAP_8BIT));
    if (!decomp) {
        ESP_LOGE(TAG, "failed to allocate tinfl decompressor");
        heap_caps_free(tar);
        return false;
    }
    tinfl_init(decomp);

    size_t in_size = deflate_size;
    size_t out_size = kMaxDecompressedSize;
    const tinfl_status status = tinfl_decompress(
        decomp, deflate, &in_size, tar, tar, &out_size,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    heap_caps_free(decomp);

    if (status != TINFL_STATUS_DONE) {
        ESP_LOGE(TAG, "gzip decompression failed (status %d)", static_cast<int>(status));
        heap_caps_free(tar);
        return false;
    }
    const size_t tar_size = out_size;

    if (!index_tar(tar, tar_size)) {
        ESP_LOGE(TAG, "no files found in embedded archive");
        heap_caps_free(tar);
        return false;
    }

    s_www_data = tar;
    ESP_LOGI(TAG, "www store ready: %zu file(s), %zu bytes in PSRAM", s_entries.size(), tar_size);
    return true;
}

bool www_store_read(const char* path, std::string& out) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_www_data) return false;
    for (const auto& e : s_entries) {
        if (e.name == path) {
            out.assign(reinterpret_cast<const char*>(e.data), e.size);
            return true;
        }
    }
    return false;
}

} // namespace hw
