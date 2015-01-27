#!/bin/sh
#自动编译扩展
rm -rf ngx_http_auto_thumb_module.c 
svn update --username=lijianwei --password=lijianwei
rm -rf /usr/local/tengine2/modules/ngx_http_auto_thumb_module.so
/usr/local/tengine2/bin/dso-tool  --add-module=/home/test/lijianwei/share_config/nginx --dst=/usr/local/tengine2/modules
/usr/local/tengine2/sbin/nginx -s stop
/usr/local/tengine2/sbin/nginx
