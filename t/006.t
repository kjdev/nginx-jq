use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== unescape query 1
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '$q | tostring';
}
--- request
GET /t?q=%7B%22name%22%3A%22jq%22%7D
--- response_body
"{\"name\":\"jq\"}"
--- error_code: 200

=== unescape query 2
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '$q | tostring';
}
--- request
GET /t?q={\x22name\x22:\x22jq\x22}
--- response_body
"{\\x22name\\x22:\\x22jq\\x22}"
--- error_code: 200
