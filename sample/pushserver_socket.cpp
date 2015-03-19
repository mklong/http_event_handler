#include "pushserver_socket.h"
#include "log/srlog.h"

int pushserver_socket::init()
{
	//��ʼ�������̳߳�
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
	//��Ҫ���ɵ��̳߳���
	r->reserved = eh;
	
	tp_.put_task(r);

	return;
}

