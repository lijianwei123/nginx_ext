
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
	ngx_http_upstream_conf_t upstream;
	ngx_int_t   			 index;
	ngx_unit_t  			 gzip_flag;
} ngx_http_memcached_loc_conf_t;

typedef struct {
	size_t  				rest;
	ngx_http_request_t     *request;
	ngx_str_t				key;
} ngx_http_memcached_ctx_t;

static ngx_int_t ngx_http_memcached_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_memcached_reint_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_memcached_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_memcached_filter_init(void *data);
static ngx_int_t ngx_http_memcached_filter(void *data, ssize_t bytes);
static void ngx_http_memcached_abort_request(ngx_http_request_t *r);
static void ngx_http_memcached_finalize_request(ngx_http_request_t *r,
	ngx_int_t rc);

static void *ngx_http_memcached_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_memcached_merge_loc_conf(ngx_conf *cf, void *parent, void *child);

static char *ngx_http_memcached_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_conf_bitmask_t ngx_http_memcached_next_upstream_masks[] = {
		{ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
		{ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
		{ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER},
		{ngx_string("not_found"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
		{ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
		{ngx_null_string, 0}
};


static ngx_command_t ngx_http_memcached_commands[] = {
		{ngx_string("memcached_pass"),
		NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
		ngx_http_memcached_pass,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
		},
		{ngx_string("memcached_bind"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_http_upstream_bind_set_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_memcached_loc_conf_t, upstream.local),
		NULL
		},
		{ngx_string("memcached_connect_timeout"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		 ngx_conf_set_msec_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, upstream.connect_timeout),
		 NULL
		},
		{ngx_string("memcached_send_timeout"),
	     NGX_HTTP_MAIN_CONF|NGX_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	     ngx_conf_set_msec_slot,
	     NGX_HTTP_LOC_CONF_OFFSET,
	     offsetof(ngx_http_memcached_loc_conf_t, upstream.send_timeout),
	     NULL
		},
		{ngx_string("memcached_buffer_size"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		 ngx_conf_set_size_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, upstream.buffer_size),
		 NULL
		},
		{ngx_string("memcached_read_timeout"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		 ngx_conf_set_msec_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, upstream.read_timeout),
		 NULL
		},
		{ngx_string("memcached_next_upstream"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
		 ngx_conf_set_bitmask_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, upstream.next_upstream),
		 &ngx_http_memcached_next_upstream_masks
		},
		{ngx_string("memcached_gzip_flag"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		 ngx_conf_set_num_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, gzip_flag),
		 NULL
		},
		{ngx_string("memcached_upstream_tries"),
		 NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		 ngx_conf_set_num_slot,
		 NGX_HTTP_LOC_CONF_OFFSET,
		 offsetof(ngx_http_memcached_loc_conf_t, upstream.upstream_tries),
		 NULL
		},
		ngx_null_command
};

static ngx_http_module_t ngx_http_memcached_module_ctx = {
		NULL,
		NULL,

		NULL,
		NULL,

		NULL,
		NULL,

		ngx_http_memcached_create_loc_conf,
		ngx_http_memcached_merge_loc_conf,
};

ngx_module_t ngx_http_memcached_module = {
		NGX_MODULE_V1,
		&ngx_http_memcached_module_ctx,
		ngx_http_memcached_commands,
		NGX_HTTP_MODULE,
	    NULL,                                  /* init master */
	    NULL,                                  /* init module */
	    NULL,                                  /* init process */
	    NULL,                                  /* init thread */
	    NULL,                                  /* exit thread */
	    NULL,                                  /* exit process */
	    NULL,                                  /* exit master */
	    NGX_MODULE_V1_PADDING
};

static ngx_str_t ngx_http_memcached_key = ngx_string("memcached_key");

#define NGX_HTTP_MEMCACHED_END (sizeof(ngx_http_memcached_end) - 1)
static u_char ngx_http_memcached_end[] = CRLF "END" CRLF;

static ngx_int_t
ngx_http_memcached_handler(ngx_http_request_t *r)
{
	ngx_int_t rc;
	ngx_http_upstream_t *u;
	ngx_http_memcached_ctx_t *ctx;
	ngx_http_memcached_loc_conf_t *mlcf;

	if(!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD)))  {
		return NGX_HTTP_NOT_ALLOWED;
	}

	rc = ngx_http_discard_request_body(r);

	if(rc != NGX_OK) {
		return rc;
	}

	if(ngx_http_set_content_type(r) != NGX_OK) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	if(ngx_http_upstream_create(r) != NGX_OK) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	u = r->upstream;

	ngx_str_set(&u->schema, "memcached://");
	u->output.tag = (ngx_buf_tag_t) &ngx_http_memcached_module;

	mlcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_module);

	u->conf = &mlcf->upstream;

	u->create_request = ngx_http_memcached_create_request;
	u->reinit_request = ngx_http_memcached_reint_request;
	u->process_header = ngx_http_memcached_process_header;
	u->abort_request = ngx_http_memcached_abort_request;
	u->finalize_request = ngx_http_memcached_finalize_request;

	ctx = ngx_palloc(r->pool, sizeof(ngx_http_memcached_ctx_t));
	if(ctx == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	ctx->rest = NGX_HTTP_MEMCACHED_END;
	ctx->request = r;

	ngx_http_set_ctx(r, ctx, ngx_http_memcached_module);

	u->input_filter_init = ngx_http_memcached_filter_init;
	u->input_filter = ngx_http_memcached_filter;
	u->input_filter_ctx = ctx;

	r->main->count++;

	ngx_http_upstream_init(r);

	return NGX_DONE;
}

static ngx_int_t
ngx_http_memcached_create_request(ngx_http_request_t *r)
{
	size_t len;
	uintptr_t escape;
	ngx_buf_t *b;
	ngx_chain_t *cl;
	ngx_http_memcached_ctx_t *ctx;
	ngx_http_variable_value_t *vv;
	ngx_http_memcached_loc_conf_t *mlcf;

	mlcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_module);

	vv = ngx_http_get_indexed_variable(r, mlcf->index);

	if(vv == NULL || vv->not_found || vv->len == 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "the \"$memcached_key\" variable is not set");
		return NGX_ERROR;
	}

	escape = 2 * ngx_escape_uri(NULL, vv->data, vv->len, NGX_ESCAPE_MEMCACHED);

}

static char *
ngx_http_memcached_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_memcached_loc_conf_t *mlcf = conf;

	ngx_str_t *value;
	ngx_url_t u;
	ngx_http_core_loc_conf_t *clcf;

	if(mlcf->upstream.upstream) {
		return "is duplicate";
	}

	value = cf->args->elts;

	ngx_memzero(&u, sizeof(ngx_url_t));

	u.url = value[1];
	u.no_resolve = 1;

	mlcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
	if(mlcf->upstream.upstream == NULL) {
		return NGX_CONF_ERROR;
	}

	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

	clcf->handler = ngx_http_memcached_handler;

	if(clcf->name.data[clcf->name.len -1] == '/') {
		clcf->auto_redirect = 1;
	}

	mlcf->index = ngx_http_get_variable_index(cf, &ngx_http_memcached_key);

	if(mlcf->index == NGX_ERROR) {
		return NGX_CONF_ERROR;
	}
	return NGX_CONF_OK;
}

static void *
ngx_http_memcached_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_memcached_loc_conf_t *conf;
	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_memcached_loc_conf_t));
	if(conf == NULL) {
		return NULL;
	}

	conf->upstream.local = NGX_CONF_UNSET_PTR;
	conf->upstream.content_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

	conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

	conf->upstream.upstream_tries = NGX_CONF_UNSET_UINT;

	conf->upstream.cyclic_temp_file = 0;
	conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;

    conf->index = NGX_CONF_UNSET;
    conf->gzip_flag = NGX_CONF_UNSET_UINT;

    return conf;
}

static char *
ngx_http_memcached_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_memcached_loc_conf_t *prev = parent;
	ngx_http_memcached_loc_conf_t *conf = child;

	ngx_conf_merge_ptr_value(conf->upstream.local, prev->upstream.local, NULL);

	ngx_conf_merge_msec_value(conf->upstream.connect_timeout, prev->upstream.connect_timeout, 60000);


    ngx_conf_merge_size_value(conf->upstream.buffer_size,
                              prev->upstream.buffer_size,
                              (size_t) ngx_pagesize);

    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                              prev->upstream.next_upstream,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_ERROR
                               |NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if(conf->upstream.upstream == NULL) {
    	conf->upstream.upstream = prev->upstream.upstream;
    }

    return NGX_CONF_OK;
}


