#ifndef __PUSHSERVER_SOCKET_H__
#define __PUSHSERVER_SOCKET_H__

#include "process_base/http_socket.h"

#include "http/http_request.h"
#include "push_task_tp.h"

using namespace http;

class pushserver_socket :public http_socket
{
public:
	pushserver_socket(){};
	~pushserver_socket(){};

	int init();
	int fini();

	virtual void on_task(http_request *r ,http_eh *eh);

protected:

private:
	push_task_tp tp_;
};

//服务单体
typedef ACE_Unmanaged_Singleton<pushserver_socket, ACE_Null_Mutex> _pushserver_socket;

#endif /* __PUSHSERVER_SOCKET_H__ */
