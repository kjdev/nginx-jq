
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include <jq.h>

typedef struct {
  jv json;
  jv library_paths;
} ngx_http_jq_srv_conf_t;

typedef struct {
  ngx_flag_t raw;
  ngx_flag_t sort;
  ngx_flag_t override_variable;
  ngx_array_t *variables;
  ngx_str_t filter;
} ngx_http_jq_loc_conf_t;

typedef struct {
  ngx_buf_t *buf;
  ngx_chain_t out;
  ngx_chain_t **last_out;
} ngx_http_jq_output_t;

typedef struct {
  ngx_str_t key;
  ngx_http_complex_value_t value;
  ngx_uint_t final;
} ngx_http_jq_variable_t;

static char *ngx_http_jq_conf_set_json_file(ngx_conf_t *, ngx_command_t *, void *);
static char *ngx_http_jq_conf_set_library_path(ngx_conf_t *, ngx_command_t *, void *);
static char *ngx_http_jq_conf_set_filter(ngx_conf_t *, ngx_command_t *, void *);
static char *ngx_http_jq_conf_set_variable(ngx_conf_t *, ngx_command_t *, void *);

static void *ngx_http_jq_create_srv_conf(ngx_conf_t *);
static void *ngx_http_jq_create_loc_conf(ngx_conf_t *);
static char *ngx_http_jq_merge_loc_conf(ngx_conf_t *, void *, void *);

static void ngx_http_jq_exit_process(ngx_cycle_t *);

static ngx_int_t ngx_http_jq_handler(ngx_http_request_t *);

static ngx_command_t ngx_http_jq_commands[] = {
  {
    ngx_string("jq_json_file"),
    NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
    ngx_http_jq_conf_set_json_file,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL
  },
  {
    ngx_string("jq_library_path"),
    NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
    ngx_http_jq_conf_set_library_path,
    NGX_HTTP_SRV_CONF_OFFSET,
    0,
    NULL
  },
  {
    ngx_string("jq_filter"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
    ngx_http_jq_conf_set_filter,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    ngx_http_jq_handler
  },
  {
    ngx_string("jq_raw"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_jq_loc_conf_t, raw),
    NULL
  },
  {
    ngx_string("jq_sort"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_jq_loc_conf_t, sort),
    NULL
  },
  {
    ngx_string("jq_override_variable"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_jq_loc_conf_t, override_variable),
    NULL
  },
  {
    ngx_string("jq_set_variable"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE23,
    ngx_http_jq_conf_set_variable,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_jq_loc_conf_t, variables),
    NULL
  },
  ngx_null_command
};

static ngx_http_module_t ngx_http_jq_module_ctx = {
  NULL,                        /* preconfiguration */
  NULL,                        /* postconfiguration */
  NULL,                        /* create main configuration */
  NULL,                        /* init main configuration */
  ngx_http_jq_create_srv_conf, /* create server configuration */
  NULL,                        /* merge server configuration */
  ngx_http_jq_create_loc_conf, /* create location configuration */
  ngx_http_jq_merge_loc_conf   /* merge location configuration */
};

ngx_module_t ngx_http_jq_module = {
  NGX_MODULE_V1,
  &ngx_http_jq_module_ctx,  /* module context */
  ngx_http_jq_commands,     /* module directives */
  NGX_HTTP_MODULE,          /* module type */
  NULL,                     /* init master */
  NULL,                     /* init module */
  NULL,                     /* init process */
  NULL,                     /* init thread */
  NULL,                     /* exit thread */
  ngx_http_jq_exit_process, /* exit process */
  NULL,                     /* exit master */
  NGX_MODULE_V1_PADDING
};


static char *
ngx_http_jq_conf_set_json_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_jq_srv_conf_t *jscf;
  ngx_str_t *value;

  jscf = conf;

  if (jv_get_kind(jscf->json) != JV_KIND_INVALID) {
    return "is duplicated";
  }

  value = cf->args->elts;

  if (value[1].len == 0) {
    return "is empty";
  }

  jscf->json = jv_load_file((char *)value[1].data, 0);
  if (!jv_is_valid(jscf->json)) {
    return "failed to load";
  }

  jscf->json = jv_array_get(jv_copy(jscf->json), 0);

  return NGX_CONF_OK;
}

static char *
ngx_http_jq_conf_set_library_path(ngx_conf_t *cf,
                                  ngx_command_t *cmd, void *conf)
{
  ngx_http_jq_srv_conf_t *jscf;
  ngx_str_t *value;

  jscf = conf;

  value = cf->args->elts;

  if (value[1].len == 0) {
    return "is empty";
  }

  jscf->library_paths = jv_array_append(jscf->library_paths,
                                        jv_string((char *)value[1].data));

  return NGX_CONF_OK;
}

static char *
ngx_http_jq_conf_set_filter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;
  ngx_http_jq_loc_conf_t *jlcf;
  ngx_str_t *value;

  jlcf = conf;

  if (jlcf->filter.len != 0) {
    return "is duplicated";
  }

  value = cf->args->elts;

  jlcf->filter.len = value[1].len;
  jlcf->filter.data = value[1].data;

  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_jq_handler;

  return NGX_CONF_OK;
}

static char *
ngx_http_jq_conf_set_variable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_jq_loc_conf_t *jlcf;
  ngx_http_jq_variable_t *var;
  ngx_array_t **variables;
  ngx_http_compile_complex_value_t ccv;
  ngx_str_t *value;

  jlcf = conf;

  variables = (ngx_array_t **) ((char *)jlcf + cmd->offset);

  if (*variables == NGX_CONF_UNSET_PTR || *variables == NULL) {
    *variables = ngx_array_create(cf->pool, 4, sizeof(ngx_http_jq_variable_t));
    if (*variables == NULL) {
      return NGX_CONF_ERROR;
    }
  }

  var = ngx_array_push(*variables);
  if (var == NULL) {
    return NGX_CONF_ERROR;
  }

  value = cf->args->elts;

  var->key = value[1];
  var->final = 0;

  if (value[2].len == 0) {
    ngx_memzero(&var->value, sizeof(ngx_http_complex_value_t));
  } else {
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &var->value;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
      return NGX_CONF_ERROR;
    }
  }

  if (cf->args->nelts == 3) {
    return NGX_CONF_OK;
  }

  if (ngx_strcmp(value[3].data, "final") != 0) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[3]);
    return NGX_CONF_ERROR;
  }

  var->final = 1;

  return NGX_CONF_OK;
}

static void *
ngx_http_jq_create_srv_conf(ngx_conf_t *cf)
{
  ngx_http_jq_srv_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_jq_srv_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->json = jv_invalid();
  conf->library_paths = jv_array();

  return conf;
}

static void *
ngx_http_jq_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_jq_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_jq_loc_conf_t));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }

  conf->raw = NGX_CONF_UNSET;
  conf->sort = NGX_CONF_UNSET;
  conf->variables = NGX_CONF_UNSET_PTR;
  conf->override_variable = NGX_CONF_UNSET;

  return conf;
}

static char *
ngx_http_jq_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_jq_loc_conf_t *prev = parent;
  ngx_http_jq_loc_conf_t *conf = child;

  ngx_conf_merge_str_value(conf->filter, prev->filter, ".");

  ngx_conf_merge_value(conf->raw, prev->raw, 0);
  ngx_conf_merge_value(conf->sort, prev->sort, 0);

  ngx_conf_merge_ptr_value(conf->variables, prev->variables, NULL);
  ngx_conf_merge_value(conf->override_variable, prev->override_variable, 1);

  return NGX_CONF_OK;
}


static void
ngx_http_jq_exit_process(ngx_cycle_t *cycle)
{
  ngx_http_jq_srv_conf_t *cf;
  ngx_http_conf_ctx_t *ctx;

  if (!cycle->conf_ctx[ngx_http_module.index]) {
    return;
  }

  ctx = (ngx_http_conf_ctx_t *)cycle->conf_ctx[ngx_http_module.index];
  if (!ctx->srv_conf[ngx_http_jq_module.ctx_index]) {
    return;
  }

  cf = (ngx_http_jq_srv_conf_t *)ctx->srv_conf[ngx_http_jq_module.ctx_index];
  if (cf) {
    jv_free(cf->json);
    jv_free(cf->library_paths);
  }
}


static void
ngx_http_jq_conf_arguments(ngx_http_request_t *r,
                           ngx_http_jq_loc_conf_t *cf, jv *arguments)
{
  ngx_http_jq_variable_t *var;
  ngx_str_t value;
  ngx_uint_t i;
  jv key, val;

  if (!cf->variables) {
    return;
  }

  var = cf->variables->elts;

  for (i = 0; i < cf->variables->nelts; i++) {
    if (var[i].key.len == 0) {
      continue;
    }

    if (ngx_http_complex_value(r, &var[i].value, &value) != NGX_OK) {
      ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                    "\"%s\" argument is undefined", var[i].key.data);
      continue;
    }

    key = jv_string_sized((const char *)var[i].key.data, var[i].key.len);
    val = jv_string_sized((const char *)value.data, value.len);

    if (!jv_object_has(jv_copy(*arguments), jv_copy(key)) || var[i].final) {
      *arguments = jv_object_set(jv_copy(*arguments),
                                 jv_copy(key), jv_copy(val));
    }

    jv_free(val);
    jv_free(key);
  }
}

static void
ngx_http_jq_query_arguments(ngx_http_request_t *r,
                            ngx_http_jq_loc_conf_t *cf, jv *arguments)
{
  jv args, data, key, val;
  size_t data_len, j;

  args = jv_string_split(jv_string_sized((const char *)r->args.data,
                                         r->args.len), jv_string("&"));
  jv_array_foreach(args, i, arg) {
    data = jv_string_split(jv_copy(arg), jv_string("="));
    data_len = jv_array_length(jv_copy(data));
    if (data_len < 1) {
      continue;
    }

    key = jv_array_get(jv_copy(data), 0);
    val = jv_array_get(jv_copy(data), 1);
    if (data_len > 2) {
      for (j = 2; j < data_len; j++) {
        val = jv_string_append_str(jv_copy(val), "=");
        val = jv_string_concat(jv_copy(val), jv_array_get(jv_copy(data), j));
      }
    }

    *arguments = jv_object_set(jv_copy(*arguments), jv_copy(key), jv_copy(val));

    jv_free(val);
    jv_free(key);
    jv_free(data);
    jv_free(arg);
  }

  jv_free(args);
}

static ngx_int_t
ngx_http_jq_output(ngx_http_request_t *r,
                   ngx_http_jq_output_t *output, const char *data, size_t len)
{
  ngx_str_t var;
  ngx_chain_t *cl;
  u_char *str;

  output->buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
  if (output->buf == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  var.len = len + 1;
  var.data = (u_char *)data;

  str = ngx_pstrdup(r->pool, &var);
  if (str == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }
  str[var.len - 1] = '\n';

  output->buf->pos = str;
  output->buf->last = str + var.len;
  output->buf->memory = 1;

  if (r->headers_out.content_length_n == -1) {
    r->headers_out.content_length_n += var.len + 1;
  } else {
    r->headers_out.content_length_n += var.len;
  }

  if (output->last_out == NULL) {
    output->out.buf = output->buf;
    output->last_out = &(output->out).next;
    output->out.next = NULL;
  } else {
    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cl->buf = output->buf;

    *(output->last_out) = cl;
    output->last_out = &cl->next;
    cl->next = NULL;
  }

  return NGX_HTTP_OK;
}

static void
ngx_http_jq_error_cb(void *data, jv err)
{}

static ngx_int_t
ngx_http_jq_handler(ngx_http_request_t *r)
{
  int jv_dump_flags = 0;
  ngx_http_jq_loc_conf_t *jlcf;
  ngx_http_jq_srv_conf_t *jscf;
  ngx_http_jq_output_t output;
  ngx_int_t rc;
  jq_state *jq;
  jv arguments, result;

  jlcf = ngx_http_get_module_loc_conf(r, ngx_http_jq_module);
  jscf = ngx_http_get_module_srv_conf(r, ngx_http_jq_module);

  output.buf = NULL;
  output.last_out = NULL;

  if (jlcf->sort) {
    jv_dump_flags = JV_PRINT_SORTED;
  }

  r->headers_out.status = NGX_HTTP_OK;

  // parse jq arguments
  arguments = jv_object();
  ngx_http_jq_query_arguments(r, jlcf, &arguments);
  ngx_http_jq_conf_arguments(r, jlcf, &arguments);

  // do the jq filter
  jq = jq_init();

  jq_set_error_cb(jq, ngx_http_jq_error_cb, NULL);

  if (jv_array_length(jv_copy(jscf->library_paths)) > 0) {
    jq_set_attr(jq, jv_string("JQ_LIBRARY_PATH"), jscf->library_paths);
  }

  if (!jq_compile_args(jq,
                       (const char *)jlcf->filter.data, jv_copy(arguments))) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "failed to jq compile: \"%s\"",
                  (const char *) jlcf->filter.data);
    jv_free(arguments);
    jq_teardown(&jq);
    return NGX_HTTP_BAD_REQUEST;
  }

  jq_start(jq, jv_copy(jscf->json), 0);

  result = jv_invalid();

  while (1) {
    if (!jv_is_valid(result = jq_next(jq))) {
      if (jv_invalid_has_msg(jv_copy(result))) {
        result = jv_invalid_get_msg(jv_copy(result));
        if (jv_get_kind(result) != JV_KIND_STRING) {
          result = jv_dump_string(result, 0);
        }
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "failed to jq execute: \"%s\"",
                      jv_string_value(jv_copy(result)));
        rc = ngx_http_jq_output(r, &output,
                                jv_string_value(jv_copy(result)),
                                jv_string_length_bytes(jv_copy(result)));
        if (rc != NGX_HTTP_OK) {
          jv_free(result);
          jv_free(arguments);
          jq_teardown(&jq);
          return rc;
        }

        r->headers_out.status = 406; /* Not Acceptable */
        r->headers_out.content_type_len = sizeof("text/plain") - 1;
        ngx_str_set(&r->headers_out.content_type, "text/plain");
      }
      break;
    }

    if (!jlcf->raw || jv_get_kind(result) != JV_KIND_STRING) {
      result = jv_dump_string(result, jv_dump_flags);
    }

    rc = ngx_http_jq_output(r, &output,
                            jv_string_value(jv_copy(result)),
                            jv_string_length_bytes(jv_copy(result)));
    if (rc != NGX_HTTP_OK) {
      jv_free(result);
      jv_free(arguments);
      jq_teardown(&jq);
      return rc;
    }

    jv_free(result);
  }

  jv_free(result);
  jv_free(arguments);
  jq_teardown(&jq);

  // output
  if (r->headers_out.content_length_n == -1) {
    return NGX_HTTP_NOT_FOUND;
  }

  if (r->method == NGX_HTTP_HEAD) {
    r->header_only = 1;
    r->headers_out.content_length_n = 0;
  }

  if (ngx_http_set_content_type(r) != NGX_OK) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  rc = ngx_http_send_header(r);
  if (rc != NGX_OK || r->header_only) {
    return rc;
  }

  if (output.buf != NULL) {
    output.buf->last_in_chain = 1;
    output.buf->last_buf = 1;
  }

  return ngx_http_output_filter(r, &(output.out));
}
