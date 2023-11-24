#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <microhttpd.h>
#include <json-c/json.h>

#define PORT 8888
#define R(x) return x
#define E(x,y) (x==y)
#define Z(x) (E(x,0))
#define N(x) (!x)
typedef char sqlchar;

static int createFuckingTable(void *_r1) {
  sqlite3 *db = (sqlite3 *)_r1;
  int rc;
  sqlchar *err_msg = NULL;
  char* sql = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS `data` (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, age INT, school TEXT, isProtected INT)");
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  sqlite3_free(sql);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    R(MHD_NO);
  }
  R(MHD_YES);
}

// POST
static int create_data(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
  sqlite3 *db = (sqlite3 *)cls;
  int rc;
  char *err_msg = NULL;
  json_object *parsed_json = json_tokener_parse(value);
  json_object *name, *age, *school, *isProtected;
  if (!json_object_object_get_ex(parsed_json, "name", &name) ||
      !json_object_object_get_ex(parsed_json, "age", &age) ||
      !json_object_object_get_ex(parsed_json, "school", &school) ||
      !json_object_object_get_ex(parsed_json, "isProtected", &isProtected)) {
        json_object_put(parsed_json);
        R(MHD_NO); // JSON 형식 에러
  }
  char *sql = sqlite3_mprintf("INSERT INTO data (name, age, school, isProtected) VALUES (%Q, %d, %Q, %d)",
    json_object_get_string(name),
    json_object_get_int(age),
    json_object_get_string(school),
    json_object_get_int(isProtected));
  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  sqlite3_free(sql);
  json_object_put(parsed_json);
  if (N(E(rc,SQLITE_OK)))
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    R(MHD_NO); // SQLite 실행 에러
  }
  R(MHD_YES); // 성공
}

static int read_callback(void *data, int argc, char **argv, char **azColName)
{
  json_object *json_row = json_object_new_object();
  for (int i = 0; i < argc; i++)
    json_object_object_add(json_row, azColName[i], json_object_new_string(argv[i] ? argv[i] : "NULL"));
  json_object *result_array = (json_object *)data;
  json_object_array_add(result_array, json_row);
  R(0);
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
    R(MHD_HTTP_INTERNAL_SERVER_ERROR);
  }
  *json_response = strdup(json_object_to_json_string(result_array));
  json_object_put(result_array);
  R(MHD_HTTP_OK);
}

// PUT
int update_data(sqlite3 *db, const char *json_data)
{
  json_object *parsed_json = json_tokener_parse(json_data);
  if (parsed_json == NULL) R(MHD_HTTP_BAD_REQUEST);
  json_object *id, *name, *age, *school, *isProtected;
  if (!json_object_object_get_ex(parsed_json, "id", &id) ||
      !json_object_object_get_ex(parsed_json, "name", &name) ||
      !json_object_object_get_ex(parsed_json, "age", &age) ||
      !json_object_object_get_ex(parsed_json, "school", &school) ||
      !json_object_object_get_ex(parsed_json, "isProtected", &isProtected))
  {
    json_object_put(parsed_json);
    R(MHD_HTTP_BAD_REQUEST);
  }
  char *sql = sqlite3_mprintf("UPDATE data SET name = %Q, age = %d, school = %Q, isProtected = %d WHERE id = %d",
    json_object_get_string(name),
    json_object_get_int(age),
    json_object_get_string(school),
    json_object_get_int(isProtected),
    json_object_get_int(id)
  );
  char *err_msg = NULL;
  int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
  sqlite3_free(sql);
  json_object_put(parsed_json);
  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    R(MHD_HTTP_INTERNAL_SERVER_ERROR);
  }
  R(MHD_HTTP_OK);
}

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
    R(MHD_HTTP_INTERNAL_SERVER_ERROR);
  }
  R(MHD_HTTP_OK);
}

static int handle_request(void *cls, struct MHD_Connection *connection,
  const char *url, const char *method,
  const char *version, const char *upload_data,
  size_t *upload_data_size, void **con_cls)
{
  sqlite3 *db = (sqlite3 *)cls;
  char *json_response;
  char *err_msg = NULL;
  if (Z(strcmp(method, "GET")))
  {
    if (Z(strcmp(url, "/"))) // root 경로에 대한 요청인 경우
    {
      // index.html 파일을 읽는다
      FILE *file = fopen("index.html", "r");
      if (file == NULL)
      {
        fprintf(stderr, "Cannot open index.html\n");
        R(MHD_NO);
      }
      fseek(file, 0, SEEK_END);
      long filesize = ftell(file);
      fseek(file, 0, SEEK_SET);
      char *filedata = (char*)calloc(1,filesize + 1);
      fread(filedata, 1, filesize, file);
      fclose(file);
      filedata[filesize] = '\0';
      struct MHD_Response *response = MHD_create_response_from_buffer(filesize, (void*)filedata, MHD_RESPMEM_MUST_FREE);
      int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
      MHD_destroy_response(response);
      R(ret);
    } else {
      int id = atoi(url + 1);
      int result = read_data(db, id, &json_response);
      if (result != MHD_HTTP_OK) R(MHD_NO);
      struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json_response), (void*)json_response, MHD_RESPMEM_MUST_FREE);
      int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
      MHD_destroy_response(response);
      R(ret);
    }
  } else if (Z(strcmp(method, "POST"))) {
    if (N(Z(*upload_data_size))) {
      char name[256], school[256];
      int age, isProtected;
      sscanf(upload_data, "name=%s&age=%d&school=%s&isProtected=%d", name, &age, school, &isProtected);
      char *sql = sqlite3_mprintf("INSERT INTO data (name, age, school, isProtected) VALUES (%Q, %d, %Q, %d)", name, age, school, isProtected);
      int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
      sqlite3_free(sql);
      if (rc != SQLITE_OK)
      {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        R(MHD_NO);
      }
      *upload_data_size = 0;
      R(MHD_YES);
    } else R(MHD_NO);
  } else if (Z(strcmp(method, "PUT"))) {
    if (N(Z(*upload_data_size))) {
      update_data(db, upload_data);
      *upload_data_size = 0;
      R(MHD_YES);
    } else R(MHD_NO);
  } else if (Z(strcmp(method, "DELETE"))) {
    int id = atoi(url + 1); // url의 첫번째 문자 '/' 제거
    int result = delete_data(db, id);
    if (result != MHD_HTTP_OK) R(MHD_NO);
    struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    R(ret);
  } else R(MHD_NO);
  R(MHD_YES);
}

int main()
{
  struct MHD_Daemon *daemon;
  sqlite3 *db;
  if (sqlite3_open("data.db", &db))
  {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    R(1);
  }
  daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT, NULL, NULL,
    &handle_request, db, MHD_OPTION_END);
  if (NULL == daemon)
  {
    sqlite3_close(db);
    R(1);
  }
  createFuckingTable((void*)db);
  printf("Server running on port %d\n", PORT);
  getchar(); // 서버 실행을 유지하기 위해 대기
  MHD_stop_daemon(daemon);
  sqlite3_close(db);
  R(0);
}
