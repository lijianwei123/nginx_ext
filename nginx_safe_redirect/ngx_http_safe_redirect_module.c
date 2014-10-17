#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#ifndef NGX_HTTP_MAX_CAPTURES
#define NGX_HTTP_MAX_CAPTURES 9
#endif

typedef struct {
	ngx_array_t *allow_redirect_host;
	ngx_str_t safe_redirect_url;
} ngx_http_safe_redirect_loc_conf_t;

static ngx_int_t ngx_http_safe_redirect_init(ngx_conf_t *cf);

static void *ngx_http_safe_redirect_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_safe_redirect_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t ngx_http_safe_redirect_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_safe_redirect_header_filter(ngx_http_request_t *r);

static ngx_command_t ngx_http_safe_redirect_commands[] = {
	{
		ngx_string("allow_redirect_host"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_array_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_safe_redirect_loc_conf_t, allow_redirect_host),
		NULL
	},
	{
		ngx_string("safe_redirect_url"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_safe_redirect_loc_conf_t, safe_redirect_url),
		NULL
	},
	ngx_null_command
};


static ngx_http_module_t ngx_http_safe_redirect_module_ctx = {
		NULL, /* preconfiguration */
		ngx_http_safe_redirect_init, /* postconfiguration */

		NULL, /* create main configuration */
		NULL, /* init main configuration */

		NULL, /* create server configuration */
		NULL, /* merge server configuration */

		ngx_http_safe_redirect_create_loc_conf, /* create location configuration */
		ngx_http_safe_redirect_merge_loc_conf /* merge location configuration */
};

ngx_module_t ngx_http_safe_redirect_module = {
		NGX_MODULE_V1,
		&ngx_http_safe_redirect_module_ctx, /* module context */
		ngx_http_safe_redirect_commands, /* module directives */
		NGX_HTTP_MODULE, /* module type */
		NULL, /* init master */
		NULL, /* init module */
		NULL, /* init process */
		NULL, /* init thread */
		NULL, /* exit thread */
		NULL, /* exit process */
		NULL, /* exit master */
		NGX_MODULE_V1_PADDING
};

//获取host
static ngx_int_t
getHostByUrl(ngx_str_t *url, ngx_str_t *host,  ngx_http_request_t *r)
{
	if (url->len <= 0) {
		return NGX_ERROR;
	}

    ngx_int_t      ncaptures = (NGX_HTTP_MAX_CAPTURES + 1) * 3;
    int           *captures = ngx_palloc(r->pool, ncaptures * sizeof(int));
    ngx_int_t ret;



	u_char	errstr[NGX_MAX_CONF_ERRSTR] = {0};
	ngx_str_t err;
	err.len = NGX_MAX_CONF_ERRSTR;
	err.data = errstr;

	ngx_str_t pattern;
	ngx_int_t options;
	ngx_regex_compile_t rc;


	ngx_str_set(&pattern, "^(\\w+):\\/\\/([^/:]+)(:\\d*)?([^# ]*)");
	options = NGX_REGEX_CASELESS;

	//编译正则表达式
	rc.pattern = pattern;
	rc.pool = r->pool;
	rc.err = err;
	rc.options = options;

	if (ngx_regex_compile(&rc) != NGX_OK) {
	        ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "%V", &rc.err);
	        return NGX_ERROR;
	}

	//执行正则表达式
	ret = ngx_regex_exec(rc.regex, url, (int *)captures, ncaptures);
	if (ret == NGX_REGEX_NO_MATCHED) {
		return NGX_ERROR;
	}
	if (ret < 0) {
		ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "regex exec failed: %i on \"%V\" using \"%V\"", rc, url, &pattern);
		return NGX_ERROR;
	}
	//开始位置   结束位置   两个元素存储  @see http://blog.csdn.net/sulliy/article/details/6247155
	host->len = captures[5] - captures[4];
	host->data = &url->data[captures[4]];

	return NGX_OK;
}

static ngx_int_t
ngx_http_safe_redirect_handler(ngx_http_request_t *r)
{
	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_http_safe_redirect_handler is called");

	ngx_http_safe_redirect_loc_conf_t *safe_redirect_conf;

	ngx_int_t rc;
	ngx_buf_t *b;
	ngx_chain_t out;

	u_char echo_string[1024] = { 0 };
	ngx_uint_t content_length = 0;


	if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	safe_redirect_conf = ngx_http_get_module_loc_conf(r, ngx_http_safe_redirect_module);
	if (safe_redirect_conf->allow_redirect_host == NGX_CONF_UNSET_PTR) {
		ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0,
				"allow_redirect_host is empty");
		return NGX_DECLINED;
	}

	if (safe_redirect_conf->safe_redirect_url.len == 0) {
		ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "safe_redirect_url is empty");
		return NGX_DECLINED;
	}


	ngx_sprintf(echo_string, "safe_redirect_url: %V, allow_redirect_host nums: %d", &safe_redirect_conf->safe_redirect_url,
			safe_redirect_conf->allow_redirect_host->nelts);

	content_length = ngx_strlen(echo_string);


	rc = ngx_http_discard_request_body(r);

	if (rc != NGX_OK) {
		return rc;
	}

	ngx_str_set(&r->headers_out.content_type, "text/html");

	if (r->method == NGX_HTTP_HEAD) {
		r->headers_out.status = NGX_HTTP_OK;
		r->headers_out.content_length_n = content_length;

		return ngx_http_send_header(r);
	}

	b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	if (b == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	out.buf = b;
	out.next = NULL;

	b->pos = echo_string;
	b->last = echo_string + content_length;
	b->memory = 1;
	b->last_buf = 1;

	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_length_n = content_length;

	rc = ngx_http_send_header(r);

	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		return rc;
	}

	return ngx_http_output_filter(r, &out);
}

static ngx_int_t
ngx_http_safe_redirect_header_filter(ngx_http_request_t *r)
{
	ngx_http_safe_redirect_loc_conf_t  *srlcf = NULL;
	srlcf = ngx_http_get_module_loc_conf(r, ngx_http_safe_redirect_module);

	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "safe_redirect_url:%V", &srlcf->safe_redirect_url);


	if (srlcf->allow_redirect_host->nelts == 0 || srlcf->safe_redirect_url.len == 0) {
		return ngx_http_next_header_filter(r);
	}

	ngx_str_t  host, allow_host;
	ngx_memzero(&host, sizeof(ngx_str_t));
	ngx_memzero(&allow_host, sizeof(ngx_str_t));


	ngx_uint_t i = 0;
	uintptr_t escape;
	size_t new_loc_uri_len;
	ngx_str_t escape_uri;
	ngx_str_null(&escape_uri);

	ngx_str_t *loc_uri_ptr = &r->headers_out.location->value;

	if ((r->headers_out.status == NGX_HTTP_MOVED_PERMANENTLY || r->headers_out.status == NGX_HTTP_MOVED_TEMPORARILY)
			&& loc_uri_ptr->len) {

		ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "location start str:%s", loc_uri_ptr->data);

		if (getHostByUrl(loc_uri_ptr, &host, r) == NGX_OK) {
			for (i = 0; i < srlcf->allow_redirect_host->nelts; i++) {
				allow_host = *((ngx_str_t *)srlcf->allow_redirect_host->elts + i) ;
				if (allow_host.len == host.len && !ngx_strncasecmp(allow_host.data, host.data, host.len)) {
					return ngx_http_next_header_filter(r);
				}
			}
			//危险跳转
			escape = 2 * ngx_escape_uri(NULL, loc_uri_ptr->data, loc_uri_ptr->len, NGX_ESCAPE_URI_COMPONENT);
			escape_uri.data = ngx_pcalloc(r->pool, loc_uri_ptr->len + escape);
			escape_uri.len = loc_uri_ptr->len + escape;
			ngx_escape_uri(escape_uri.data, loc_uri_ptr->data, loc_uri_ptr->len, NGX_ESCAPE_URI_COMPONENT);

			new_loc_uri_len = loc_uri_ptr->len + escape + strlen("?link=") + srlcf->safe_redirect_url.len;

			loc_uri_ptr->data = ngx_pcalloc(r->pool, new_loc_uri_len + 1);
			loc_uri_ptr->len = ngx_sprintf(loc_uri_ptr->data, "%V?link=%V", &srlcf->safe_redirect_url, &escape_uri) - loc_uri_ptr->data;
		}
	}

	return ngx_http_next_header_filter(r);

}

static void *
ngx_http_safe_redirect_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_safe_redirect_loc_conf_t *local_conf = NULL;

	local_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_safe_redirect_loc_conf_t));
	if (local_conf == NULL) {
		return NULL;
	}

	local_conf->allow_redirect_host = NGX_CONF_UNSET_PTR;
	ngx_str_null(&local_conf->safe_redirect_url);

	return local_conf;
}

static char *
ngx_http_safe_redirect_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	 ngx_http_safe_redirect_loc_conf_t* prev = parent;
	 ngx_http_safe_redirect_loc_conf_t* conf = child;

	 ngx_log_error(NGX_LOG_EMERG, cf->log, 0, "safe_redirect_url: %V", &conf->safe_redirect_url);

	 ngx_conf_merge_str_value(conf->safe_redirect_url, prev->safe_redirect_url, "");

	 /*
	 ngx_uint_t i = 0;
	 ngx_str_t *conf_str = NULL, *prev_str = NULL;
	 if (conf->allow_redirect_host != NGX_CONF_UNSET_PTR && prev->allow_redirect_host != NGX_CONF_UNSET_PTR) {
		 //合并
		 for (i = 0; i < prev->allow_redirect_host->nelts; i++) {
			 prev_str = (ngx_str_t *)prev->allow_redirect_host->elts + i;
			 conf_str = ngx_array_push(conf->allow_redirect_host);
			 conf_str->len = prev_str->len;
			 conf_str->data = prev_str->data;
		 }
	 } else {
		 ngx_conf_merge_ptr_value(conf->allow_redirect_host, prev->allow_redirect_host, NULL);
	 }
	 */

	 ngx_conf_merge_ptr_value(conf->allow_redirect_host, prev->allow_redirect_host, NULL);

	 return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_safe_redirect_init(ngx_conf_t *cf)
{
	//header 过滤
	ngx_http_next_header_filter = ngx_http_top_header_filter;
	ngx_http_top_header_filter = ngx_http_safe_redirect_header_filter;

	return NGX_OK;


	//内容
	ngx_http_handler_pt *ptr = NULL;
	ngx_http_core_main_conf_t *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	ptr = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);

	if (ptr == NULL) {
		return NGX_ERROR;
	}

	*ptr = ngx_http_safe_redirect_handler;

	return NGX_OK;
}
