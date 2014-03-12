#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
	//是否开启
	ngx_flag_t github;
} ngx_http_github_loc_conf_t;

static ngx_int_t ngx_http_github_init(ngx_conf_t *cf);

static void *ngx_http_github_create_loc_conf(ngx_conf_t *cf);

static char *ngx_http_github_string(ngx_conf_t *cf, ngx_command_t *cmd,
		void *conf);

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



	glcf = ngx_http_get_module_loc_conf(r, ngx_http_github_module);

	if(!glcf->github) {
		return NGX_DECLINED;
	}

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
