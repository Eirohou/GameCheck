#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "steam_api.h"
#include "utils.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    char *data;
    size_t size;
} MemoryBuffer;

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer *mem = (MemoryBuffer *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0; /* Out of memory */
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

static int http_get(const char *url, char **response_out, size_t *size_out) {
    if (!url || !response_out) return -1;

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    MemoryBuffer chunk;
    chunk.data = malloc(1);
    chunk.size = 0;
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        return -1;
    }
    chunk.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "GameCompChecker/1.0 (Linux; x86_64)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        free(chunk.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        free(chunk.data);
        return -1;
    }

    *response_out = chunk.data;
    if (size_out) *size_out = chunk.size;
    return 0;
}

int steam_search_games(const char *query, SearchResultSet *results) {
    if (!query || !results) return -1;
    memset(results, 0, sizeof(SearchResultSet));

    char *encoded_query = url_encode(query);
    if (!encoded_query) return -1;

    char url[1024];
    snprintf(url, sizeof(url), "https://store.steampowered.com/api/storesearch/?term=%s&l=english&cc=US", encoded_query);
    free(encoded_query);

    char *response = NULL;
    if (http_get(url, &response, NULL) != 0 || !response) {
        return -1;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root) return -1;

    cJSON *items = cJSON_GetObjectItem(root, "items");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return 0;
    }

    int count = 0;
    int total = cJSON_GetArraySize(items);
    for (int i = 0; i < total && count < MAX_SEARCH_RESULTS; i++) {
        cJSON *item = cJSON_GetArrayItem(items, i);
        if (!item) continue;

        cJSON *id_elem = cJSON_GetObjectItem(item, "id");
        cJSON *name_elem = cJSON_GetObjectItem(item, "name");
        if (!cJSON_IsNumber(id_elem) || !cJSON_IsString(name_elem)) continue;

        SteamSearchResult *res = &results->items[count];
        res->appid = (long)id_elem->valuedouble;
        strncpy(res->name, name_elem->valuestring, sizeof(res->name) - 1);

        cJSON *platforms = cJSON_GetObjectItem(item, "platforms");
        if (cJSON_IsObject(platforms)) {
            cJSON *win = cJSON_GetObjectItem(platforms, "windows");
            cJSON *mac = cJSON_GetObjectItem(platforms, "mac");
            cJSON *lin = cJSON_GetObjectItem(platforms, "linux");
            res->has_win = cJSON_IsTrue(win);
            res->has_mac = cJSON_IsTrue(mac);
            res->has_linux = cJSON_IsTrue(lin);
        }
        count++;
    }

    results->total_results = count;
    cJSON_Delete(root);
    return count;
}

static bool has_valid_req_content(const char *html) {
    if (!html || strlen(html) < 10) return false;
    char sample[256];
    strip_html(html, sample, sizeof(sample));
    if (strlen(sample) < 10) return false;

    if (str_contains_ci(sample, "processor") ||
        str_contains_ci(sample, "memory") ||
        str_contains_ci(sample, "graphics") ||
        str_contains_ci(sample, "storage") ||
        str_contains_ci(sample, "os:")) {
        return true;
    }
    return false;
}

int steam_get_game_details(long appid, GameRequirements *details) {
    if (!details || appid <= 0) return -1;
    memset(details, 0, sizeof(GameRequirements));
    details->appid = appid;

    char url[512];
    snprintf(url, sizeof(url), "https://store.steampowered.com/api/appdetails?appids=%ld&l=english", appid);

    char *response = NULL;
    if (http_get(url, &response, NULL) != 0 || !response) {
        return -1;
    }

    cJSON *root = cJSON_Parse(response);
    free(response);
    if (!root) return -1;

    char appid_str[32];
    snprintf(appid_str, sizeof(appid_str), "%ld", appid);

    cJSON *app_obj = cJSON_GetObjectItem(root, appid_str);
    if (!cJSON_IsObject(app_obj)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *success = cJSON_GetObjectItem(app_obj, "success");
    if (!cJSON_IsTrue(success)) {
        cJSON_Delete(root);
        return -2; /* Game not found or access restricted */
    }

    cJSON *data = cJSON_GetObjectItem(app_obj, "data");
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *name = cJSON_GetObjectItem(data, "name");
    if (cJSON_IsString(name)) {
        strncpy(details->game_name, name->valuestring, sizeof(details->game_name) - 1);
    } else {
        snprintf(details->game_name, sizeof(details->game_name), "App %ld", appid);
    }

    cJSON *short_desc = cJSON_GetObjectItem(data, "short_description");
    if (cJSON_IsString(short_desc)) {
        strip_html(short_desc->valuestring, details->short_desc, sizeof(details->short_desc));
    }

    cJSON *platforms = cJSON_GetObjectItem(data, "platforms");
    if (cJSON_IsObject(platforms)) {
        cJSON *lin = cJSON_GetObjectItem(platforms, "linux");
        details->has_native_linux = cJSON_IsTrue(lin);
    }

    /* Check Linux requirements first */
    cJSON *linux_reqs = cJSON_GetObjectItem(data, "linux_requirements");
    bool found_linux = false;

    if (cJSON_IsObject(linux_reqs)) {
        cJSON *min = cJSON_GetObjectItem(linux_reqs, "minimum");
        if (cJSON_IsString(min) && has_valid_req_content(min->valuestring)) {
            details->has_minimum = true;
            details->using_native_reqs = true;
            found_linux = true;
            parse_requirement_html(min->valuestring, &details->minimum);

            cJSON *rec = cJSON_GetObjectItem(linux_reqs, "recommended");
            if (cJSON_IsString(rec) && has_valid_req_content(rec->valuestring)) {
                details->has_recommended = true;
                parse_requirement_html(rec->valuestring, &details->recommended);
            }
        }
    }

    /* Fall back to PC requirements (Wine/Proton compatibility) */
    if (!found_linux) {
        details->using_native_reqs = false;
        cJSON *pc_reqs = cJSON_GetObjectItem(data, "pc_requirements");
        if (cJSON_IsObject(pc_reqs)) {
            cJSON *min = cJSON_GetObjectItem(pc_reqs, "minimum");
            if (cJSON_IsString(min) && has_valid_req_content(min->valuestring)) {
                details->has_minimum = true;
                parse_requirement_html(min->valuestring, &details->minimum);
            }

            cJSON *rec = cJSON_GetObjectItem(pc_reqs, "recommended");
            if (cJSON_IsString(rec) && has_valid_req_content(rec->valuestring)) {
                details->has_recommended = true;
                parse_requirement_html(rec->valuestring, &details->recommended);
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}
