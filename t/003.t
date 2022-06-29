use Test::Nginx::Socket 'no_plan';

no_root_location();

run_tests();

__DATA__

=== query id=1
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t?id=1
--- response_body
{"id":1,"data":"data-1"}
--- error_code: 200

=== query id=2
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t?id=2
--- response_body
{"id":2,"data":"data-2"}
--- error_code: 200

=== query no id
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t
--- error_code: 400

=== no query
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t?id=10
--- error_code: 404

=== filter no type
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_filter '.[] | select(.id == $id)';
}
--- request
GET /t?id=1
--- error_code: 404

=== variable
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_set_variable id 3;
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t
--- response_body
{"id":3,"data":"data-3"}
--- error_code: 200

=== override variable 
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_set_variable id 3;
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t?id=2
--- response_body
{"id":2,"data":"data-2"}
--- error_code: 200

=== no override variable
--- config
jq_json_file $TEST_NGINX_JSON_DIR/items.json;
location = /t {
  jq_set_variable id 3 final;
  jq_filter '.[] | select(.id == ($id | tonumber))';
}
--- request
GET /t?id=2
--- response_body
{"id":3,"data":"data-3"}
--- error_code: 200
