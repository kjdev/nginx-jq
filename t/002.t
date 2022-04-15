use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== items.json
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.';
}
--- request
GET /t
--- response_body
[{"id":1,"data":"data-1"},{"id":2,"data":"data-2"},{"id":3,"data":"data-3"}]
--- error_code: 200

=== lines
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[]';
}
--- request
GET /t
--- response_body
{"id":1,"data":"data-1"}
{"id":2,"data":"data-2"}
{"id":3,"data":"data-3"}
--- error_code: 200

=== lines by raw
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_raw on;
  jq_filter '.[]';
}
--- request
GET /t
--- response_body
{"id":1,"data":"data-1"}
{"id":2,"data":"data-2"}
{"id":3,"data":"data-3"}
--- error_code: 200

=== lines by sort
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_sort on;
  jq_filter '.[]';
}
--- request
GET /t
--- response_body
{"data":"data-1","id":1}
{"data":"data-2","id":2}
{"data":"data-3","id":3}
--- error_code: 200
