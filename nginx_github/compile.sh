#!/bin/sh
#自动编译扩展
rm -rf ngx_http_github_module.c 
wget http://168.192.122.71:8000/ngx_http_github_module.c
rm -rf /usr/local/tengine2/modules/ngx_http_github_module.so
/usr/local/tengine2/bin/dso-tool  --add-module=/home/test/lijianwei/nginx_github --dst=/usr/local/tengine2/modules
/usr/local/tengine2/sbin/nginx -s stop
/usr/local/tengine2/sbin/nginx