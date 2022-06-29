use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== include id1
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
jq_library_path $TEST_NGINX_JSON_DIR;
location = /t {
  jq_filter 'include "id"; id1';
}
--- request
GET /t
--- response_body
{"id":1,"data":"data-1"}
--- error_code: 200

=== include id2
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
jq_library_path $TEST_NGINX_JSON_DIR;
location = /t {
  jq_filter 'include "id"; id2';
}
--- request
GET /t
--- response_body
{"id":2,"data":"data-2"}
--- error_code: 200

=== import id3
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
jq_library_path $TEST_NGINX_JSON_DIR;
location = /t {
  jq_filter 'import "id" as id; id::id3';
}
--- request
GET /t
--- response_body
{"id":3,"data":"data-3"}
--- error_code: 200
