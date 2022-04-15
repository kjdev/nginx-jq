ngix-jq
=======

This nginx module responds by filtering the json file with jq.

Dependency
----------

[jq](http://stedolan.github.io/jq) header and library.

- jv.h
- jq.so


Installation
------------

### Build install

``` sh
$ : "clone repository"
$ git clone https://github.com/kjdev/nginx-jq
$ cd nginx-jq
$ : "get nginx source"
$ NGINX_VERSION=1.x.x # specify nginx version
$ wget http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz
$ tar -zxf nginx-${NGINX_VERSION}.tar.gz
$ cd nginx-${NGINX_VERSION}
$ : "build module"
$ ./configure --add-dynamic-module=../
$ make && make install
```

Edit `nginx.conf` and load the required modules.

```
load_module modules/ngx_http_jq_module.so;
```

### Docker

``` sh
$ docker build -t nginx-jq .
$ : "app.conf: Create nginx configuration"
$ : "app.json: Create json file"
$ docker run -p 80:80 -v $PWD/app.conf:/etc/nginx/http.d/default.conf -v $PWD/app.json:/opt/app.json nginx-jq
```

> Github package: ghcr.io/kjdev/nginx-jq


Configuration Directives
------------------------

```
Syntax: jq_json_file file;
Default: -
Context: server
```

Specify the json file to be filtered for jq.

```
Syntax: jq_filter filter;
Default: jq_filter .;
Context: location
```

Specify a filter for jq.

```
Syntax: jq_raw on | off;
Default: jq_raw off;
Context: location
```

Determines whether to output raw strings instead of JSON text.

```
Syntax: jq_sort on | off;
Default: jq_sort off;
Context: location
```

Determines whether the object's keys are sorted or not.

```
Syntax: jq_set_variable field value;
Default: -
Context: location
```

Set the variable `filed` with the value `value`.

```
Syntax: jq_override_variable on | off;
Default: jq_override_variable on;
Context: location
```

Determines if the variable can be overwritten.


Example
-------

Json file: `test.json`

``` json
{
  "test": "TEST",
  "items": [
    { "id": 1, "data": "data-1" },
    { "id": 2, "data": "data-2" },
    { "id": 3, "data": "data-3" }
  ]
}
```

Nginx configuration file: `test.conf`

```
server {
  listen 80 default_server;
  server_name _;
  default_type application/json;

  jq_json_file /opt/test.json;

  location = / {
    jq_filter '.';
  }

  location = /test {
    jq_filter '.test';
  }

  location = /test/raw {
    default_type text/plain;
    jq_raw on;
    jq_filter '.test';
  }

  location = /array {
    default_type text/plain;
    jq_filter '.[]';
  }

  location = /array/raw {
    default_type text/plain;
    jq_raw on;
    jq_filter '.[]';
  }

  location = /items {
    default_type text/plain;
    jq_filter '.items[]';
  }

  location = /items/sort {
    default_type text/plain;
    jq_sort on;
    jq_filter '.items[]';
  }

  # id query parameter required
  location = /items/id {
    jq_filter '.items[] | select(.id == ($id | tonumber))';
  }

  # id variable is a string
  location = /items/id/str {
    jq_filter '.items[] | select(.id == $id)';
  }

  # id variable defaults to 2
  # id variable is overwritten with query parameter
  location = /items/id/def-2 {
    jq_set_variable id 2;
    jq_filter '.items[] | select(.id == ($id | tonumber))';
  }

  # id variable is not overwritten by query parameters
  location = /items/id/2 {
    jq_override_variable off;
    jq_set_variable id 2;
    jq_filter '.items[] | select(.id == ($id | tonumber))';
  }

  # error when running jq
  location = /error/execute {
    jq_filter '.[] | .error';
  }

  # incorrect jq filter
  location = /error/compile {
    jq_filter 'invalid';
  }
}
```

Start the server using docker.

``` sh
$ docker run --rm -p 80:80 -v $PWD/test.json:/opt/test.json -v $PWD/test.conf:/etc/nginx/http.d/default.conf ghcr.io/kjdev/nginx-jq
```

Execution result.

``` sh
$ alias run="curl -w '%{content_type} %{http_code}\n'"
$ run localhost
{"test":"TEST","items":[{"id":1,"data":"data-1"},{"id":2,"data":"data-2"},{"id":3,"data":"data-3"}]}
application/json 200
$ run localhost/test
"TEST"
application/json 200
$ run localhost/test/raw
TEST
text/plain 200
$ run localhost/array
"TEST"
[{"id":1,"data":"data-1"},{"id":2,"data":"data-2"},{"id":3,"data":"data-3"}]
text/plain 200
$ run localhost/array/raw
TEST
[{"id":1,"data":"data-1"},{"id":2,"data":"data-2"},{"id":3,"data":"data-3"}]
text/plain 200
$ run localhost/items
{"id":1,"data":"data-1"}
{"id":2,"data":"data-2"}
{"id":3,"data":"data-3"}
text/plain 200
$ run localhost/items/sort
{"data":"data-1","id":1}
{"data":"data-2","id":2}
{"data":"data-3","id":3}
text/plain 200
$ run localhost/items/id
<html><head><title>400 Bad Request</title>..
text/html 400
$ run localhost/items/id -G -d id=1
{"id":1,"data":"data-1"}
application/json 200
$ run localhost/items/id -G -d id=2
{"id":1,"data":"data-1"}
application/json 200
$ run localhost/items/id -G -d id=10
<html><head><title>404 Not Found</title>..
text/html 404
$ run localhost/items/id/str -G -d id=1
<html><head><title>404 Not Found</title>..
text/html 404
$ run localhost/items/id/def-2
{"id":2,"data":"data-2"}
application/json 200
$ run localhost/items/id/def-2 -G -d id=3
{"id":3,"data":"data-3"}
application/json 200
$ run localhost/items/id/2
{"id":2,"data":"data-2"}
application/json 200
$ run localhost/items/id/2 -G -d id=1
{"id":2,"data":"data-2"}
application/json 200
$ run localhost/error/execute
Cannot index string with string "error"
text/plain 406
$ run localhost/error/compile
<html><head><title>400 Bad Request</title>..
text/html 400
```
