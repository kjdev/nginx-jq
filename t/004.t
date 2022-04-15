use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== error execute
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter '.[] | .error';
}
--- request
GET /t
--- response_headers
Content-Type: text/plain
--- response_body
Cannot index string with string "error"
--- error_code: 406

=== error compile
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter 'invalid';
}
--- request
GET /t
--- error_code: 400

=== error compile
--- config
default_type application/json;
jq_json_file $TEST_NGINX_JSON_DIR/test.json;
location = /t {
  jq_filter 'error';
}
--- request
GET /t
--- error_code: 406
