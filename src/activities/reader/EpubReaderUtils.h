#pragma once

#include <Epub.h>
#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>

#include <optional>
#include <vector>

#include "../../BookmarkEntry.h"

namespace EpubReaderUtils {

// Filename for the persisted return point inside an EPUB's cache directory.
// Reuses the bookmark JSON format (single-element list); the file lives in the
// cache dir so it shares progress.bin's lifecycle (cleared on cache wipe).
inline constexpr const char RETURN_POINT_FILENAME[] = "/returnPoint.json";

// Persists reader progress for an EPUB to its cache directory. Returns true on success.
inline bool saveProgress(Epub& epub, int spineIndex, int pageNumber, int pageCount) {
  if (spineIndex < 0 || spineIndex > 0xFFFF || pageNumber < 0 || pageNumber > 0xFFFF || pageCount < 0 ||
      pageCount > 0xFFFF) {
    LOG_ERR("ERS", "Progress values out of range: spine=%d page=%d count=%d", spineIndex, pageNumber, pageCount);
    return false;
  }
  HalFile f;
  if (!Storage.openFileForWrite("ERS", epub.getCachePath() + "/progress.bin", f)) {
    LOG_ERR("ERS", "Could not open progress file for write!");
    return false;
  }
  uint8_t data[6];
  data[0] = spineIndex & 0xFF;
  data[1] = (spineIndex >> 8) & 0xFF;
  data[2] = pageNumber & 0xFF;
  data[3] = (pageNumber >> 8) & 0xFF;
  data[4] = pageCount & 0xFF;
  data[5] = (pageCount >> 8) & 0xFF;
  const size_t written = f.write(data, sizeof(data));
  if (written != sizeof(data)) {
    LOG_ERR("ERS", "Short write saving progress: %u/%u bytes", (unsigned)written, (unsigned)sizeof(data));
    return false;
  }
  LOG_DBG("ERS", "Progress saved: spine=%d page=%d", spineIndex, pageNumber);
  return true;
}

inline bool saveReturnPoint(Epub& epub, const BookmarkEntry& entry) {
  const std::string path = epub.getCachePath() + RETURN_POINT_FILENAME;
  std::vector<BookmarkEntry> single{entry};
  if (!JsonSettingsIO::saveBookmarks(single, path.c_str())) {
    LOG_ERR("ERS", "Failed to save return point to %s", path.c_str());
    return false;
  }
  LOG_DBG("ERS", "Return point saved: %s (%.2f%%)", entry.xpath.c_str(), entry.percentage);
  return true;
}

inline std::optional<BookmarkEntry> loadReturnPoint(const Epub& epub) {
  const std::string path = epub.getCachePath() + RETURN_POINT_FILENAME;
  if (!Storage.exists(path.c_str())) {
    return std::nullopt;
  }
  const String json = Storage.readFile(path.c_str());
  if (json.isEmpty()) {
    LOG_DBG("ERS", "Return point file empty; removing %s", path.c_str());
    Storage.remove(path.c_str());
    return std::nullopt;
  }
  std::vector<BookmarkEntry> single;
  if (!JsonSettingsIO::loadBookmarks(single, json.c_str()) || single.empty()) {
    LOG_DBG("ERS", "Return point parse failed/empty; removing %s", path.c_str());
    Storage.remove(path.c_str());
    return std::nullopt;
  }
  LOG_DBG("ERS", "Return point loaded: %s (%.2f%%)", single.front().xpath.c_str(), single.front().percentage);
  return single.front();
}

inline void clearReturnPoint(const Epub& epub) {
  const std::string path = epub.getCachePath() + RETURN_POINT_FILENAME;
  if (Storage.remove(path.c_str())) {
    LOG_DBG("ERS", "Return point cleared");
  }
}

}  // namespace EpubReaderUtils
