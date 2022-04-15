use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== test.json
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter '.';
}
--- request
GET /t
--- response_headers
Content-Type: application/json
--- response_body
{"test":"TEST"}
--- error_code: 200

=== specify key
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter '.test';
}
--- request
GET /t
--- response_headers
Content-Type: application/json
--- response_body
"TEST"
--- error_code: 200

=== specify key by raw
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  default_type text/plain;
  jq_raw on;
  jq_filter '.test';
}
--- request
GET /t
--- response_headers
Content-Type: text/plain
--- response_body
TEST
--- error_code: 200

=== no existent key
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter '.error';
}
--- request
GET /t
--- response_headers
Content-Type: application/json
--- response_body
null
--- error_code: 200
