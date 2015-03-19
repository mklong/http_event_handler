#include "pushserver_socket.h"
#include "log/srlog.h"

int pushserver_socket::init()
{
	//初始化处理线程池
	tp_.init(16);
	return 0;
}

int pushserver_socket::fini()
{
	tp_.fini();
	return 0;
}

void pushserver_socket::on_task(http_request *r ,http_eh *eh )
{
	srlog_tracer(pushserver_socket::on_task);
	//需要分派到线程池中
	r->reserved = eh;
	
	tp_.put_task(r);

	return;
}

