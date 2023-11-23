#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <microhttpd.h>
#include <json-c/json.h>

#define PORT 8888

// POST
static int create_data(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
    sqlite3 *db = (sqlite3 *)cls;
    int rc;
    char *err_msg = NULL;

    // JSON 데이터 파싱
    json_object *parsed_json = json_tokener_parse(value);
    json_object *name, *age, *school, *isProtected;

    // JSON 객체에서 필요한 데이터 추출
    if (!json_object_object_get_ex(parsed_json, "name", &name) ||
        !json_object_object_get_ex(parsed_json, "age", &age) ||
        !json_object_object_get_ex(parsed_json, "school", &school) ||
        !json_object_object_get_ex(parsed_json, "isProtected", &isProtected))
    {
        json_object_put(parsed_json);
        return MHD_NO; // JSON 형식 에러
    }

    // SQL 쿼리 구성 및 실행
    char *sql = sqlite3_mprintf("INSERT INTO data (name, age, school, isProtected) VALUES (%Q, %d, %Q, %d)",
                                json_object_get_string(name),
                                json_object_get_int(age),
                                json_object_get_string(school),
                                json_object_get_int(isProtected));

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    sqlite3_free(sql);
    json_object_put(parsed_json);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return MHD_NO; // SQLite 실행 에러
    }

    return MHD_YES; // 성공
}

static int read_callback(void *data, int argc, char **argv, char **azColName)
{
    json_object *json_row = json_object_new_object();

    for (int i = 0; i < argc; i++)
    {
        json_object_object_add(json_row, azColName[i], json_object_new_string(argv[i] ? argv[i] : "NULL"));
    }

    json_object *result_array = (json_object *)data;
    json_object_array_add(result_array, json_row);

    return 0;
}

// GET
int read_data(sqlite3 *db, int id, char **json_response)
{
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT * FROM data WHERE id = %d;", id);

    json_object *result_array = json_object_new_array();
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, read_callback, result_array, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        json_object_put(result_array);
        return MHD_HTTP_INTERNAL_SERVER_ERROR;
    }

    *json_response = strdup(json_object_to_json_string(result_array));
    json_object_put(result_array);

    return MHD_HTTP_OK;
}

// PUT
int update_data(sqlite3 *db, const char *json_data)
{
    json_object *parsed_json = json_tokener_parse(json_data);
    if (parsed_json == NULL)
    {
        return MHD_HTTP_BAD_REQUEST;
    }

    json_object *id, *name, *age, *school, *isProtected;
    if (!json_object_object_get_ex(parsed_json, "id", &id) ||
        !json_object_object_get_ex(parsed_json, "name", &name) ||
        !json_object_object_get_ex(parsed_json, "age", &age) ||
        !json_object_object_get_ex(parsed_json, "school", &school) ||
        !json_object_object_get_ex(parsed_json, "isProtected", &isProtected))
    {
        json_object_put(parsed_json);
        return MHD_HTTP_BAD_REQUEST;
    }

    char *sql = sqlite3_mprintf("UPDATE data SET name = %Q, age = %d, school = %Q, isProtected = %d WHERE id = %d",
                                json_object_get_string(name),
                                json_object_get_int(age),
                                json_object_get_string(school),
                                json_object_get_int(isProtected),
                                json_object_get_int(id));

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    sqlite3_free(sql);
    json_object_put(parsed_json);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return MHD_HTTP_INTERNAL_SERVER_ERROR;
    }

    return MHD_HTTP_OK;
}

// DELETE
#include <microhttpd.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

int delete_data(sqlite3 *db, int id)
{
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM data WHERE id = %d;", id);

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return MHD_HTTP_INTERNAL_SERVER_ERROR;
    }

    return MHD_HTTP_OK;
}

static int handle_request(void *cls, struct MHD_Connection *connection,
                          const char *url, const char *method,
                          const char *version, const char *upload_data,
                          size_t *upload_data_size, void **con_cls)
{
    sqlite3 *db = (sqlite3 *)cls;

    if (strcmp(method, "GET") == 0)
    {
        // GET
    }
    else if (strcmp(method, "POST") == 0)
    {
        // POST
        // upload_data에서 JSON 데이터 추출 및 create_data 함수 호출
    }
    else if (strcmp(method, "PUT") == 0)
    {
        // PUT
        // upload_data에서 JSON 데이터 추출 및 update_data 함수 호출
    }
    else if (strcmp(method, "DELETE") == 0)
    {
        // DELETE
        // URL에서 ID 추출 및 delete_data 함수 호출
    }
    else
    {
        // 예외처리
    }

    return MHD_YES;
}

int main()
{
    struct MHD_Daemon *daemon;
    sqlite3 *db;

    // 데이터베이스 연결
    if (sqlite3_open("data.db", &db))
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT, NULL, NULL,
                              &handle_request, db, MHD_OPTION_END);

    if (NULL == daemon)
    {
        sqlite3_close(db);
        return 1;
    }

    printf("Server running on port %d\n", PORT);
    getchar(); // 서버 실행을 유지하기 위해 대기

    MHD_stop_daemon(daemon);
    sqlite3_close(db);

    return 0;
}