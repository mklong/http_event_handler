#include "push_task_tp.h"
#include "log/srlog.h"

//test
char * response = 
	"HTTP/1.1 200 OK\r\n"
	"Server: Nginx-VoiceCloudProxy nginx/1.4.1\r\n"
	"Date: Tue, 16 Dec 2014 06:51:02 GMT\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 143\r\n"
	"Last-Modified: Sun, 28 Sep 2014 09:08:53 GMT\r\n"
	"Connection: keep-alive\r\n"
	"ETag: \"5427d025-97\"\r\n"
	"Accept-Ranges: bytes\r\n"
	"\r\n"
	"<html>"
	"<head>"
	"<title>Welcome to nginx!</title>"
	"</head>"
	"<body bgcolor=\"white\" text=\"black\">"
	"<center><h1>Welcome to nginx!</h1></center>"
	"</body>"
	"</html>"
	;

int push_task_tp::svc( void )
{	
	srlog_tracer(push_task_tp::svc);
	while (1){

		http_request	*r = NULL;
	
		r= queue_.getq();
		
		if (r == NULL){
			if (stop_thrd_){
				break;
			}else{
				continue;
			}
		}

		srlog_info("push_task_tp::svc | get request: %p.",r);

		http_eh * eh = r->reserved;
	
		//´¦ÀíÂß¼­

		ngx_buf_t* buf =  ngx_create_temp_buf(r->pool,strlen(response));

		ngx_memcpy(buf->pos,response,strlen(response));
		buf->last = buf->pos + strlen(response);

		r->send_chain.buf = buf;

		//ÏìÓ¦
		//eh->response_mt(r,false);
		eh->response_mt(NULL,false);

		srlog_info("push_task_tp::svc | request done : %p.",r);

	}
	

}


