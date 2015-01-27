#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#ifndef NGX_HTTP_MAX_CAPTURES
#define NGX_HTTP_MAX_CAPTURES 9
#endif

typedef struct {
	ngx_flag_t auto_thumb_enabled;
	//配置服务器
	ngx_str_t  config_server;
	//上传服务器
	ngx_str_t  upload_server;

} ngx_http_auto_thumb_loc_conf_t;

typedef struct {
	ngx_uint_t width;
	ngx_uint_t height;
} thumb_size_t;

static ngx_int_t ngx_http_auto_thumb_init(ngx_conf_t *cf);

static void *ngx_http_auto_thumb_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_auto_thumb_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


static ngx_int_t ngx_http_auto_thumb_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_auto_thumb_header_filter(ngx_http_request_t *r);

static ngx_command_t ngx_http_auto_thumb_commands[] = {
	{
		ngx_string("auto_thumb_enabled"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_auto_thumb_loc_conf_t, auto_thumb_enabled),
		NULL
	},
	{
		ngx_string("config_server"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_auto_thumb_loc_conf_t, config_server),
		NULL
	},
	{
		ngx_string("upload_server"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_auto_thumb_loc_conf_t, upload_server),
		NULL
	},
	ngx_null_command
};


static ngx_http_module_t ngx_http_auto_thumb_module_ctx = {
		NULL, /* preconfiguration */
		ngx_http_auto_thumb_init, /* postconfiguration */

		NULL, /* create main configuration */
		NULL, /* init main configuration */

		NULL, /* create server configuration */
		NULL, /* merge server configuration */

		ngx_http_auto_thumb_create_loc_conf, /* create location configuration */
		ngx_http_auto_thumb_merge_loc_conf /* merge location configuration */
};

ngx_module_t ngx_http_auto_thumb_module = {
		NGX_MODULE_V1,
		&ngx_http_auto_thumb_module_ctx, /* module context */
		ngx_http_auto_thumb_commands, /* module directives */
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

/**
 * 获取宽度、高度
 * http://f3.v.veimg.cn/company_picture/001/856/188/logo/14072133814137_100X100.png
 * return 100  100
 */
static ngx_int_t getThumbSizeByUrl(ngx_str_t *url_ptr, thumb_size_t *thumb_size_ptr)
{
	if (url_ptr->len == 0) {
		return NGX_ERROR;
	}

	char *start, *end, *middle = NULL;
	const char lsep = '@';
	const char rsep = '.';
	const char msep = 'X';

	char width[5] = {0}, height[5] = {0};

	start = strrchr((const char *)url_ptr->data, (int)lsep);
	end   = strrchr((const char *)url_ptr->data, (int)rsep);
	middle = strrchr((const char *)url_ptr->data, (int)msep);
	if (NULL == start || NULL == end || NULL == middle) {
		return NGX_ERROR;
	}

	memcpy(width, start, middle - start);
	memcpy(height, ++middle, end - middle);

	thumb_size_ptr->width = atoi((const char *)width);
	thumb_size_ptr->height = atoi((const char *)height);

	return NGX_OK;
}

static ngx_int_t
ngx_http_auto_thumb_handler(ngx_http_request_t *r)
{
	ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "ngx_http_auto_thumb_handler is called");

	ngx_int_t ret = -1;

	if (r->method & (NGX_HTTP_HEAD | NGX_HTTP_GET)) {
		return NGX_HTTP_NOT_ALLOWED;
	}

	ngx_http_auto_thumb_loc_conf_t *auto_thumb_conf =  NULL;
	auto_thumb_conf = ngx_http_get_module_loc_conf(r, ngx_http_auto_thumb_module);
	if (auto_thumb_conf->auto_thumb_enabled == 0) {
		return NGX_DECLINED;
	}

	thumb_size_t *thumb_size_ptr = ngx_palloc(r->pool, sizeof(thumb_size_t));
	ret = getThumbSizeByUrl(r->uri, thumb_size_ptr);

	if (ret != NGX_OK) {
		return NGX_ERROR;
	}
	//调用缩略图生成程序

	//upload server








}

static void *
ngx_http_auto_thumb_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_auto_thumb_loc_conf_t *local_conf = NULL;

	local_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_auto_thumb_loc_conf_t));
	if (local_conf == NULL) {
		return NULL;
	}

	local_conf->auto_thumb_enabled = NGX_CONF_UNSET;
	ngx_str_null(&local_conf->config_server);
	ngx_str_null(&local_conf->upload_server);

	return local_conf;
}

static char *
ngx_http_auto_thumb_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	 ngx_http_auto_thumb_loc_conf_t *prev = parent;
	 ngx_http_auto_thumb_loc_conf_t *conf = child;

	 ngx_conf_merge_value(conf->auto_thumb_enabled, prev->auto_thumb_enabled, 0);
	 ngx_conf_merge_str_value(conf->config_server, prev->config_server, "");
	 ngx_conf_merge_str_value(conf->upload_server, prev->upload_server, "");

	 return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_auto_thumb_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt *handler;
	ngx_http_core_main_conf_t *cmcf;
	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_auto_thumb_module);

	handler = ngx_array_push(cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if (handler == NULL) {
		return NGX_ERROR;
	}

	*handler = ngx_http_auto_thumb_handler;

	return NGX_OK;
}
