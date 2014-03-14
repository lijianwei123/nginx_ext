#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
	//是否开启
	ngx_flag_t github;
} ngx_http_github_loc_conf_t;

static ngx_int_t ngx_http_github_init(ngx_conf_t *cf);

static void *ngx_http_github_create_loc_conf(ngx_conf_t *cf);

static ngx_int_t
ngx_http_get_value(ngx_http_request_t *r, ngx_str_t *get_var, ngx_array_t *get_values);

static ngx_command_t ngx_http_github_commands[] = {
		{
				ngx_string("github"),
				NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
				ngx_conf_set_flag_slot,
				NGX_HTTP_LOC_CONF_OFFSET,
				offsetof(ngx_http_github_loc_conf_t, github),
				NULL
		},
		ngx_null_command
};


static ngx_http_module_t ngx_http_github_module_ctx = {NULL, /* preconfiguration */
ngx_http_github_init, /* postconfiguration */

NULL, /* create main configuration */
NULL, /* init main configuration */

NULL, /* create server configuration */
NULL, /* merge server configuration */

ngx_http_github_create_loc_conf, /* create location configuration */
NULL /* merge location configuration */
};

ngx_module_t ngx_http_github_module = { NGX_MODULE_V1,
		&ngx_http_github_module_ctx, /* module context */
		ngx_http_github_commands, /* module directives */
		NGX_HTTP_MODULE, /* module type */
		NULL, /* init master */
		NULL, /* init module */
		NULL, /* init process */
		NULL, /* init thread */
		NULL, /* exit thread */
		NULL, /* exit process */
		NULL, /* exit master */
		NGX_MODULE_V1_PADDING };

static ngx_int_t ngx_http_github_handler(ngx_http_request_t *r) {
	ngx_http_github_loc_conf_t *glcf;
	u_char resp_content[1024];

	ngx_array_t *get_values;
	ngx_str_t *value;
	ngx_str_t get_var;

	ngx_uint_t content_length = 0;
	ngx_chain_t out;
	ngx_buf_t *b;

	ngx_int_t rc;



	glcf = ngx_http_get_module_loc_conf(r, ngx_http_github_module);

	if(!glcf->github) {
		return NGX_DECLINED;
	}

	//ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "nginx get args:%V", &r->args);

	ngx_memzero(resp_content, sizeof(resp_content));

	get_values = ngx_palloc(r->pool, sizeof(ngx_array_t));
	if(get_values == NULL) {
		return NGX_ERROR;
	}


	ngx_str_set(&get_var, "id");

	//初始化数组
	ngx_array_init(get_values, r->pool, 10, sizeof(ngx_str_t));

	if(ngx_http_get_value(r, &get_var, get_values) != NGX_OK) {
		return NGX_ERROR;
	}

	fprintf(stderr, "%d", get_values->nelts);

	value = get_values->elts;


	ngx_sprintf(resp_content, "respo:%V\n", value);

	//输出
	content_length = ngx_strlen(resp_content);

	//限定get  post
	if(!(r->method & (NGX_HTTP_GET | NGX_HTTP_POST | NGX_HTTP_HEAD))) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	ngx_str_set(&r->headers_out.content_type, "text/html");

	//如果是head请求
	if(r->method == NGX_HTTP_HEAD) {
		r->headers_out.status = NGX_HTTP_OK;
		r->headers_out.content_length_n = content_length;

		return ngx_http_send_header(r);
	}

	b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
	if(b == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	out.buf = b;
	out.next = NULL;

	b->pos = resp_content;
	b->last = resp_content + content_length;
	b->memory = 1;
	b->last_buf = 1;

	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_length_n = content_length;

	rc = ngx_http_send_header(r);
	if(rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		return rc;
	}

	return ngx_http_output_filter(r, &out);
}

//获取get值
static ngx_int_t
ngx_http_get_value(ngx_http_request_t *r, ngx_str_t *get_var, ngx_array_t *get_values)
{
	ngx_str_t *v, arg;
	u_char *p, *pos, *last;


	v = ngx_array_push(get_values);
	if(v == NULL) {
		return NGX_ERROR;
	}

	arg.len = get_var->len + 1;
	arg.data = ngx_palloc(r->pool, arg.len + 1);
	ngx_memzero(arg.data, sizeof(arg.data));

	pos = ngx_copy(arg.data, get_var->data, get_var->len);
	last = ngx_copy(pos, "=", strlen("="));

	p = ngx_strlcasestrn(r->args.data, r->args.data + r->args.len - 1, arg.data, arg.len);

	pos = p + arg.len;

	//*(r->args.data + r->args.len - 1) = '\0';

	//fprintf(stderr, "%s", r->args.data);

	last = ngx_strlchr(pos, r->args.data + r->args.len - 1, '&');

	v->data = ngx_palloc(r->pool, last - pos);
	v->len = last - pos;

	last = ngx_copy(v->data, pos, v->len);


	fprintf(stderr, "test%s", "lijianwei");

	return NGX_OK;
}


static void *
ngx_http_github_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_github_loc_conf_t *local_conf = NULL;
	local_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_github_loc_conf_t));

	if(local_conf == NULL) {
		return NULL;
	}

	local_conf->github = NGX_CONF_UNSET;

	return local_conf;
}



static ngx_int_t
ngx_http_github_init(ngx_conf_t *cf) {
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);

	if(h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_http_github_handler;

	return NGX_OK;
}
