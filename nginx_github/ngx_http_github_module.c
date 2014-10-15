#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
	//是否开启
	ngx_flag_t github;
} ngx_http_github_loc_conf_t;


typedef struct {
	ngx_array_t *git_down_status;
} ngx_http_github_ctx_t;

static ngx_int_t ngx_http_github_init(ngx_conf_t *cf);

static void *ngx_http_github_create_loc_conf(ngx_conf_t *cf);

static ngx_int_t
ngx_http_get_value(ngx_http_request_t *r, ngx_str_t *get_var,
		ngx_array_t *get_values);

static ngx_command_t ngx_http_github_commands[] = { { ngx_string("github"),
		NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
				| NGX_CONF_FLAG, ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_github_loc_conf_t, github),
		NULL }, ngx_null_command };

static ngx_http_module_t ngx_http_github_module_ctx = { NULL, /* preconfiguration */
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

	ngx_http_github_ctx_t *ctx;
	ngx_hash_t *git_down_status;

	ngx_str_t *temp;

	ngx_hash_init_t hash_init;


	glcf = ngx_http_get_module_loc_conf(r, ngx_http_github_module);

	if (!glcf->github) {
		return NGX_DECLINED;
	}

	//ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "nginx get args:%V", &r->args);

	ngx_memzero(resp_content, sizeof(resp_content));

	get_values = ngx_palloc(r->pool, sizeof(ngx_array_t));
	if (get_values == NULL) {
		return NGX_ERROR;
	}

	ngx_str_set(&get_var, "id");

	//初始化数组
	ngx_array_init(get_values, r->pool, 10, sizeof(ngx_str_t));

	if (ngx_http_get_value(r, &get_var, get_values) != NGX_OK) {
		return NGX_ERROR;
	}

	fprintf(stderr, "%d", get_values->nelts);

	value = get_values->elts;

	ctx = ngx_http_get_module_ctx(r, ngx_http_github_module);
	if(ctx == NULL) {
		//初始化
		ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_github_ctx_t));
		if(ctx == NULL) {
			return NGX_ERROR;
		}



		ctx->git_down_status = git_down_status;

		ngx_http_set_ctx(r, ctx, ngx_http_github_module);
	}

	git_down_status = ctx->git_down_status;



	ngx_sprintf(resp_content, "respo:%V\n", value);

	//输出
	content_length = ngx_strlen(resp_content);

	//限定get  post
	if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_POST | NGX_HTTP_HEAD))) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	ngx_str_set(&r->headers_out.content_type, "text/html");

	//如果是head请求
	if (r->method == NGX_HTTP_HEAD) {
		r->headers_out.status = NGX_HTTP_OK;
		r->headers_out.content_length_n = content_length;

		return ngx_http_send_header(r);
	}

	b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
	if (b == NULL) {
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
	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		return rc;
	}

	return ngx_http_output_filter(r, &out);
}

//获取get值
static ngx_int_t ngx_http_get_value(ngx_http_request_t *r, ngx_str_t *get_var,
		ngx_array_t *get_values) {
	ngx_str_t *v, arg;
	u_char *p, *pos, *last;

	v = ngx_array_push(get_values);
	if (v == NULL) {
		return NGX_ERROR;
	}

	arg.len = get_var->len + 1;
	arg.data = ngx_palloc(r->pool, arg.len + 1);
	ngx_memzero(arg.data, sizeof(arg.data));

	pos = ngx_copy(arg.data, get_var->data, get_var->len);
	last = ngx_copy(pos, "=", strlen("="));

	p = ngx_strlcasestrn(r->args.data, r->args.data + r->args.len - 1,
			arg.data, arg.len);

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

//获取post值
//@see http://wiki.nginx.org/HttpFormInputModule
static ngx_int_t ngx_http_post_value(ngx_http_request_t *r,
		ngx_str_t *post_var, ngx_array_t *post_values) {
	u_char *p, *v, *last, *buf;
	ngx_chain_t *cl;
	size_t len = 0;
	ngx_array_t *array = NULL;
	ngx_str_t *s;
	ngx_buf_t *b;

	if (multi) {
		array = ngx_array_create(r->pool, 1, sizeof(ngx_str_t));
		if (array == NULL) {
			return NGX_ERROR;
		}
		value->data = (u_char *) array;
		value->len = sizeof(ngx_array_t);

	} else {
		value->data = NULL;
		value->len = 0;
	}

	/* we read data from r->request_body->bufs */
	if (r->request_body == NULL || r->request_body->bufs == NULL) {
		dd("empty rb or empty rb bufs");
		return NGX_OK;
	}

	if (r->request_body->bufs->next != NULL) {
		/* more than one buffer...we should copy the data out... */
		len = 0;
		for (cl = r->request_body->bufs; cl; cl = cl->next) {
			b = cl->buf;

			if (b->in_file) {
				ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
						"form-input: in-file buffer found. aborted. "
							"consider increasing your client_body_buffer_size "
							"setting");

				return NGX_OK;
			}

			len += b->last - b->pos;
		}

		dd("len=%d", (int) len);

		if (len == 0) {
			return NGX_OK;
		}

		buf = ngx_palloc(r->pool, len);
		if (buf == NULL) {
			return NGX_ERROR;
		}

		p = buf;
		last = p + len;

		for (cl = r->request_body->bufs; cl; cl = cl->next) {
			p = ngx_copy(p, cl->buf->pos, cl->buf->last - cl->buf->pos);
		}

		dd("p - buf = %d, last - buf = %d", (int) (p - buf), (int) (last - buf));

		dd("copied buf (len %d): %.*s", (int) len, (int) len, buf);

	} else {
		dd("XXX one buffer only");

		b = r->request_body->bufs->buf;
		if (ngx_buf_size(b) == 0) {
			return NGX_OK;
		}

		buf = b->pos;
		last = b->last;
	}

	for (p = buf; p < last; p++) {
		/* we need '=' after name, so drop one char from last */

		p = ngx_strlcasestrn(p, last - 1, arg_name, arg_len - 1);
		if (p == NULL) {
			return NGX_OK;
		}

		dd("found argument name, offset: %d", (int) (p - buf));

		if ((p == buf || *(p - 1) == '&') && *(p + arg_len) == '=') {
			v = p + arg_len + 1;
			dd("v = %d...", (int) (v - buf));

			dd("buf now (len %d): %.*s", (int) (last - v), (int) (last - v), v);
			p = ngx_strlchr(v, last, '&');
			if (p == NULL) {
				dd("& not found, pointing it to last...");
				p = last;

			} else {
				dd("found &, pointing it to %d...", (int) (p - buf));
			}

			if (multi) {
				s = ngx_array_push(array);
				if (s == NULL) {
					return NGX_ERROR;
				}
				s->data = v;
				s->len = p - v;
				dd("array var:%.*s", (int) s->len, s->data);

			} else {
				value->data = v;
				value->len = p - v;
				dd("value: [%.*s]", (int) value->len, value->data);
				return NGX_OK;
			}
		}
	}
	/*
	 if (multi) {
	 value->data = (u_char *) array;
	 value->len = sizeof(ngx_array_t);
	 }
	 */
	return NGX_OK;

}

static void *
ngx_http_github_create_loc_conf(ngx_conf_t *cf) {
	ngx_http_github_loc_conf_t *local_conf = NULL;
	local_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_github_loc_conf_t));

	if (local_conf == NULL) {
		return NULL;
	}

	local_conf->github = NGX_CONF_UNSET;

	return local_conf;
}

static ngx_int_t ngx_http_github_init(ngx_conf_t *cf) {
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);

	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_http_github_handler;

	return NGX_OK;
}
