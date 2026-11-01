#pragma once
#include "iss_data.h"

/// Fetch current ISS position from wheretheiss.at API.
/// Returns true on success, populates `pos` with parsed data.
bool fetchIssPosition(IssPosition &pos);

/// Fetch current astronauts in space from Open Notify API.
/// Returns true on success, populates `crew` with parsed data.
bool fetchCrew(CrewData &crew);

/// Enrich existing crew data with bios, nationalities, images from TheSpaceDevs.
/// Matches crew members by name (fuzzy). Returns true if any member was enriched.
bool fetchCrewEnrichment(CrewData &crew);

/// Download a JPEG avatar image from a URL into a PSRAM buffer.
/// On success, sets *outData and *outSize. Caller owns the buffer (free with free()).
bool fetchAvatarImage(const char* url, uint8_t** outData, size_t* outSize);

/// Fetch the current ISS two-line element set (wheretheiss.at, falling back
/// to CelesTrak). Lines are validated (format + checksum) before returning.
/// `line1` / `line2` must each hold at least 70 bytes.
bool fetchIssTle(char* line1, char* line2, size_t bufSize);
