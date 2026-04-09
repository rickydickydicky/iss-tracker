#include "api_client.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool fetchIssPosition(IssPosition &pos) {
    HTTPClient http;
    http.begin(API_ISS_POSITION);
    http.setTimeout(5000);

    int code = http.GET();
    if (code != 200) {
        LOG_E("ISS position API returned %d", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        LOG_E("JSON parse error: %s", err.c_str());
        return false;
    }

    pos.latitude   = doc["latitude"].as<float>();
    pos.longitude  = doc["longitude"].as<float>();
    pos.altitude   = doc["altitude"].as<float>();
    pos.velocity   = doc["velocity"].as<float>();
    pos.visibility = doc["visibility"].as<String>();
    pos.footprint  = doc["footprint"].as<float>();
    pos.timestamp  = doc["timestamp"].as<unsigned long>();
    pos.valid      = true;

    LOG_I("ISS @ %.2f, %.2f | Alt: %.0f km | Vel: %.0f km/h | %s",
          pos.latitude, pos.longitude, pos.altitude,
          pos.velocity, pos.visibility.c_str());

    return true;
}

// ─── Helper: map location string to short craft name ────
// corquaid API uses full names like "International Space Station"
static String normalizeCraft(const String &raw) {
    // ISS — matches "ISS", "International Space Station", spacecraft names
    // like "Crew-9 Dragon", "Soyuz MS-27", "Progress MS-29"
    if (raw.indexOf("ISS") >= 0 ||
        raw.indexOf("International Space Station") >= 0 ||
        raw.indexOf("Dragon") >= 0 ||
        raw.indexOf("Soyuz") >= 0 ||
        raw.indexOf("Starliner") >= 0 ||
        raw.indexOf("Progress") >= 0 ||
        raw.indexOf("Crew") >= 0) {
        return "ISS";
    }
    // Chinese Space Station
    if (raw.indexOf("Tiangong") >= 0 ||
        raw.indexOf("CSS") >= 0 ||
        raw.indexOf("Shenzhou") >= 0 ||
        raw.indexOf("Tianzhou") >= 0) {
        return "Tiangong";
    }
    LOG_W("Unrecognized craft/location: '%s'", raw.c_str());
    return raw;
}

// ─── Helper: try fetching from a single crew endpoint ───
static bool tryFetchCrew(const char* url, CrewData &crew) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);

    LOG_I("Trying crew API: %s", url);

    int code = http.GET();
    if (code != 200) {
        LOG_E("Crew API returned %d from %s", code, url);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        LOG_E("Crew JSON parse error: %s", err.c_str());
        return false;
    }

    crew.count = 0;
    crew.issCount = 0;
    JsonArray people = doc["people"].as<JsonArray>();

    for (JsonObject person : people) {
        if (crew.count >= 20) break;

        crew.members[crew.count].name = person["name"].as<String>();

        // Handle all known API formats:
        //   Open Notify:  { "name": "...", "craft": "ISS" }
        //   corquaid:     { "name": "...", "location": "International Space Station" }
        //                 or { "name": "...", "spacecraft": "Crew-9 Dragon" }
        //   iqfareez:     { "name": "...", "craft": "ISS" }
        String craft;
        if (person["craft"].is<const char*>()) {
            craft = person["craft"].as<String>();
        } else if (person["location"].is<const char*>()) {
            craft = person["location"].as<String>();
        } else if (person["spacecraft"].is<const char*>()) {
            craft = person["spacecraft"].as<String>();
        } else {
            // Log all keys on this person object to help debug
            String keys = "";
            for (JsonPair kv : person) {
                keys += kv.key().c_str();
                keys += ",";
            }
            LOG_W("Unknown crew JSON keys for '%s': %s",
                  person["name"].as<const char*>(), keys.c_str());
            craft = "Unknown";
        }
        crew.members[crew.count].craft = normalizeCraft(craft);

        if (crew.members[crew.count].craft == "ISS") {
            crew.issCount++;
        }
        crew.count++;
    }

    if (crew.count == 0) {
        // JSON parsed but no people found — try "number" field to check
        int num = doc["number"] | 0;
        LOG_W("Crew JSON had 0 people entries (number field: %d)", num);
        return false;
    }

    crew.valid = true;
    LOG_I("People in space: %d (%d on ISS) from %s", crew.count, crew.issCount, url);
    return true;
}

bool fetchCrew(CrewData &crew) {
    // Try primary endpoint first (corquaid — GitHub-hosted, reliable)
    if (tryFetchCrew(API_ASTROS, crew)) {
        return true;
    }

    LOG_W("Primary crew API failed, trying fallback...");

    // Try fallback (Open Notify — sometimes down)
    if (tryFetchCrew(API_ASTROS_FALLBACK, crew)) {
        return true;
    }

    LOG_E("All crew API endpoints failed");
    return false;
}

// ═══════════════════════════════════════════════════════
//  Crew Enrichment (TheSpaceDevs API)
// ═══════════════════════════════════════════════════════

// Normalize a name for fuzzy matching:
// lowercase, strip middle initials (single letters), remove periods
static String normalizeName(const String &name) {
    String n = name;
    n.toLowerCase();
    n.trim();
    String result;
    int start = 0;
    for (int i = 0; i <= (int)n.length(); i++) {
        if (i == (int)n.length() || n.charAt(i) == ' ') {
            String word = n.substring(start, i);
            word.replace(".", "");
            word.trim();
            if (word.length() > 1) {  // Skip single-letter initials
                if (result.length() > 0) result += " ";
                result += word;
            }
            start = i + 1;
        }
    }
    return result;
}

// Find the best crew index matching an API name. Returns -1 if no match.
static int findCrewMatch(const CrewData &crew, const String &apiName) {
    String normApi = normalizeName(apiName);

    // Exact normalized match
    for (int i = 0; i < crew.count; i++) {
        if (normalizeName(crew.members[i].name) == normApi) return i;
    }

    // Fallback: unique last-name match
    int lastSp = normApi.lastIndexOf(' ');
    if (lastSp > 0) {
        String lastName = normApi.substring(lastSp + 1);
        int matchCount = 0, matchIdx = -1;
        for (int i = 0; i < crew.count; i++) {
            String cn = normalizeName(crew.members[i].name);
            int sp = cn.lastIndexOf(' ');
            String cl = (sp > 0) ? cn.substring(sp + 1) : cn;
            if (cl == lastName) { matchCount++; matchIdx = i; }
        }
        if (matchCount == 1) return matchIdx;
    }
    return -1;
}

bool fetchCrewEnrichment(CrewData &crew) {
    HTTPClient http;
    http.begin(API_SPACEDEVS);
    http.setTimeout(12000);

    LOG_I("[Enrich] Fetching from TheSpaceDevs...");
    int code = http.GET();
    if (code != 200) {
        LOG_E("[Enrich] SpaceDevs returned %d", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        LOG_E("[Enrich] JSON parse error: %s", err.c_str());
        return false;
    }

    JsonArray results = doc["results"].as<JsonArray>();
    if (!results) {
        LOG_E("[Enrich] No 'results' array in response");
        return false;
    }

    int matched = 0;
    for (JsonObject person : results) {
        String apiName = person["name"].as<String>();
        int idx = findCrewMatch(crew, apiName);
        if (idx < 0) {
            LOG_W("[Enrich] No match for '%s'", apiName.c_str());
            continue;
        }

        CrewMember &m = crew.members[idx];

        // Bio (truncate to 500 chars to save memory)
        String bio = person["bio"].as<String>();
        if (bio == "null") bio = "";
        if (bio.length() > 500) bio = bio.substring(0, 497) + "...";
        m.bio = bio;

        // Nationality — could be string, array of strings, or array of objects
        if (person["nationality"].is<JsonArray>()) {
            JsonArray natArr = person["nationality"].as<JsonArray>();
            if (natArr.size() > 0) {
                JsonVariant first = natArr[0];
                if (first.is<JsonObject>()) {
                    // Array of objects: [{"name": "...", "nationality_name": "..."}]
                    String nn = first["nationality_name"].as<String>();
                    if (nn.length() > 0 && nn != "null") {
                        m.nationality = nn;
                    } else {
                        m.nationality = first["name"].as<String>();
                    }
                } else {
                    // Array of strings: ["American"]
                    m.nationality = first.as<String>();
                }
            }
        } else if (person["nationality"].is<const char*>()) {
            m.nationality = person["nationality"].as<String>();
        }

        // Agency — could be an object with abbrev/name or a simple string
        if (person["agency"].is<JsonObject>()) {
            m.agency = person["agency"]["abbrev"].as<String>();
            if (m.agency.length() == 0 || m.agency == "null") {
                m.agency = person["agency"]["name"].as<String>();
            }
        } else if (person["agency"].is<const char*>()) {
            m.agency = person["agency"].as<String>();
        }

        // Image URL — prefer thumbnail, fall back to full image
        // Handle both string and object formats
        String thumbUrl, fullUrl;
        if (person["profile_image_thumbnail"].is<const char*>()) {
            thumbUrl = person["profile_image_thumbnail"].as<String>();
        }
        if (person["profile_image"].is<const char*>()) {
            fullUrl = person["profile_image"].as<String>();
        } else if (person["profile_image"].is<JsonObject>()) {
            thumbUrl = person["profile_image"]["thumbnail_url"].as<String>();
            fullUrl  = person["profile_image"]["image_url"].as<String>();
        }
        // Guard against ArduinoJson returning literal "null" for null fields
        if (thumbUrl == "null") thumbUrl = "";
        if (fullUrl == "null")  fullUrl  = "";
        m.imageUrl = (thumbUrl.length() > 4) ? thumbUrl : fullUrl;

        LOG_I("[Enrich] imageUrl for '%s': '%.80s'", m.name.c_str(),
              m.imageUrl.length() > 0 ? m.imageUrl.c_str() : "(none)");

        m.enriched = true;
        matched++;
        LOG_I("[Enrich] '%s' -> '%s' (%s, %s)",
              apiName.c_str(), m.name.c_str(),
              m.agency.c_str(), m.nationality.c_str());
    }

    LOG_I("[Enrich] Matched %d of %d crew members", matched, crew.count);
    return matched > 0;
}

// ═══════════════════════════════════════════════════════
//  Avatar Image Download
// ═══════════════════════════════════════════════════════

bool fetchAvatarImage(const char* url, uint8_t** outData, size_t* outSize) {
    if (!url || strlen(url) < 5) return false;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    // CDNs may redirect — follow them
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code != 200) {
        LOG_E("[Avatar] HTTP %d for %.60s...", code, url);
        http.end();
        return false;
    }

    int contentLen = http.getSize();  // -1 for chunked/unknown
    const int MAX_AVATAR = 150000;

    // Known length: validate range
    if (contentLen > MAX_AVATAR) {
        LOG_E("[Avatar] Image too large: %d bytes", contentLen);
        http.end();
        return false;
    }

    // Allocate buffer — use Content-Length if known, else start with 32KB
    int allocLen = (contentLen > 0) ? contentLen : 32768;
    uint8_t* buf = (uint8_t*)ps_malloc(allocLen);
    if (!buf) {
        LOG_E("[Avatar] ps_malloc(%d) failed", allocLen);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    int totalRead = 0;
    unsigned long start = millis();

    while (millis() - start < 20000) {
        int avail = stream->available();
        if (avail > 0) {
            // Grow buffer if needed (chunked transfer)
            if (totalRead + avail > allocLen) {
                int newLen = allocLen * 2;
                if (newLen > MAX_AVATAR) newLen = MAX_AVATAR;
                if (totalRead + avail > newLen) {
                    LOG_E("[Avatar] Exceeds max size during download");
                    break;
                }
                uint8_t* newBuf = (uint8_t*)ps_realloc(buf, newLen);
                if (!newBuf) {
                    LOG_E("[Avatar] ps_realloc(%d) failed", newLen);
                    break;
                }
                buf = newBuf;
                allocLen = newLen;
            }

            int toRead = (avail < (allocLen - totalRead)) ? avail : (allocLen - totalRead);
            int rd = stream->readBytes(buf + totalRead, toRead);
            totalRead += rd;

            // If we know the length and have it all, stop
            if (contentLen > 0 && totalRead >= contentLen) break;
        } else if (!http.connected()) {
            break;  // Connection closed — we have what we have
        } else {
            delay(5);
        }
    }
    http.end();

    if (totalRead < 100) {
        LOG_E("[Avatar] Too few bytes: %d", totalRead);
        free(buf);
        return false;
    }

    // Verify JPEG signature (SOI marker)
    if (buf[0] != 0xFF || buf[1] != 0xD8) {
        LOG_E("[Avatar] Not a valid JPEG (header: 0x%02X 0x%02X)", buf[0], buf[1]);
        free(buf);
        return false;
    }

    *outData = buf;
    *outSize = (size_t)totalRead;
    LOG_I("[Avatar] Downloaded %d bytes from %.50s...", totalRead, url);
    return true;
}
