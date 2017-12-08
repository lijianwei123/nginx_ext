## 安装依赖GraphicsMagick

GraphicsMagick 依赖libjpeg  libpng  freetype

可以从ftp://ftp.graphicsmagick.org/pub/GraphicsMagick/delegates 选择下载

```SHELL
wget ftp://ftp.graphicsmagick.org/pub/GraphicsMagick/delegates/jpegsrc.v9a.tar.gz
./configure && make && make install
```
```SHELL
wget http://downloads.sourceforge.net/project/graphicsmagick/graphicsmagick/1.3.20/GraphicsMagick-1.3.20.tar.gz
./configure --prefix=/usr/local/GraphicsMagick-1.3.20/ --with-jpeg=yes  --x-libraries=/usr/local/lib
make && make install
```

## 使用
/usr/local/GraphicsMagick-1.3.20/bin/gm  convert 1.jpg  -resize 200x200 2.jpg 按照比例缩放

------
## 安装官方的image filter module
```SHELL
yum -y install libjpeg-devel  libpng-devel freetype-devel

wget https://github.com/libgd/libgd/archive/gd-2.1.1.tar.gz

tar xvzf gd-2.1.1.tar.gz

rm -rf  CMakeCache.txt

cmake -DENABLE_JPEG=1  -DENABLE_PNG=1  -DENABLE_FREETYPE=1 -DCMAKE_INSTALL_PREFIX=/usr/local/libgd2.1.1

make 

make install

# 在tegine已编译过的文件中
./configure --with-http_image_filter_module=shared --with-cc-opt="-I /usr/local/libgd2.1.1/include" --with-ld-opt="-Wl,-rpath,/usr/local/libgd2.1.1/lib -L /usr/local/libgd2.1.1/lib"
make
make dso_tool

# 这样就安装了ngx_http_image_filter_module模块了
```

## 安装lua模块，增强nginx编程能力
```SHELL
wget http://luajit.org/download/LuaJIT-2.0.3.tar.gz
make && make install

./configure --with-http_lua_module=shared
make
make dso_install
```

## 修改ngx_http_image_filter_module.c

```C
	//add by lijianwei start
	#define IMAGE_INVOKE_ON  1
	#define IMAGE_INVOKE_OFF 0

	static ngx_int_t
	ngx_http_image_get_image_invoke(ngx_http_request_t *r)
	{
		ngx_str_t image_invoke_var_name = ngx_string("image_filter_invoke");
		u_char *data = ngx_palloc(r->pool, image_invoke_var_name.len);
		ngx_uint_t key = ngx_hash_strlow(data, image_invoke_var_name.data, image_invoke_var_name.len);

		ngx_http_variable_value_t  *vv = ngx_http_get_variable(r, &image_invoke_var_name, key);

		return (vv == NULL || vv->not_found) ? IMAGE_INVOKE_OFF :  ngx_atoi(vv->data, vv->len);
	}
	//add by lijianwei end

	//在ngx_http_image_header_filter  ngx_http_image_body_filter增加
	rc = ngx_http_image_get_image_invoke(r);
	if (rc == IMAGE_INVOKE_OFF) {
	    return ngx_http_next_body_filter(r, in);
	}
```


## squid默认不支持带query的url缓存，需要重写url
	修改nginx配置
	if (!-e $request_filename) {
		    rewrite "^([^!]+)!(\d+)!(\w+)\.(jpg|jpeg|gif|png|bmp)$" $1.$4?key=$3&w=$2 last;
	}
	使
	http://f3.v.veimg.cn/meadincms/1/2015/0209/20150209024130979!640!m_meadin_com.jpg
	重写为
	http://f3.v.veimg.cn/meadincms/1/2015/0209/20150209024130979?key=m_meadin_com&w=640

## 在nginx配置中增加
`
if ($arg_key) {
	set $image_filter_invoke 0;
	rewrite_by_lua '
		if ngx.var.arg_w == Nil then
			ngx.exit(500);
		else
			local res = ngx.location.capture("/memc", {args = {key = ngx.var.arg_key}});
			if res.status ~= 200 or res.body == "" or res.body ~= ngx.var.arg_w then
				ngx.exit(403);
			end
		end
		ngx.var.image_filter_invoke = 1;
	';
	image_filter resize $arg_w -;
}

location = /memc {
	internal;   #只能内部访问
	set $memcached_key  $arg_key;
	memcached_pass 168.192.122.29:11211;
	default_type     text/plain;
}
`


