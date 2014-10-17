nginx 版本安全跳转

配置支持 server  location  作用域
allow_redirect_host www.meadin.com;
allow_redirect_host www.jobbon.cn;
safe_redirect_url http://www.veryeast.cn;

安装请运行./compile.sh
