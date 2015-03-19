#ifndef __HTTP_SOCKET_H__
#define __HTTP_SOCKET_H__

#include "http_declaration.h"

#include "core/nfl/nfl_base.h"
#include "core/nfl/nfl_core_threads.h"
#include "core/nfl/nfl_accept_handler.h"
#include "core/nfl/eh_stat.h"

class http_socket
{
	friend class http_eh;
	friend class http_acceptor;
	
// 	enum HTTP_EH_MODE{
// 		HTTP_PIPELINE = 0,		/* 请求响应一一对应，如果是多个请求，需要排队处理 */
// 		HTTP_STREAM,				/* 请求响应无需对应，可以只有请求，没有响应 */
// 	};

public:
	http_socket();
	~http_socket();

	//设置工作模式，在open之前设置
	//void set_http_mode(HTTP_EH_MODE m);

	//设置数据处理回调
	

	//开启网络线程池以及listen socket
	int open(short port , size_t worker_threads, size_t timeout, 
		ACE_HANDLE acpt_hdl  =0, bool keep_alive = false,size_t max_fd = nfl_os::MaxFd);

	//停止网络模块
	int close();

	//
	virtual void on_task(http::http_request *r ,http_eh *eh);

protected:

	//用于记录或者查询eh的状态，暂不使用
	eh_state_mgr<http_eh> eh_mgr_;

	//跨线程通知事件缓存
	nfl::cq_base<http_response_eh> resp_eh_cache_;


	//交叉包含，模板类前置声明无效，使用指针替代
	http_acceptor *acceptor_;
	nfl::sock_thread_pool threadpool_;

	ACE_Atomic_Op<ACE_Thread_Mutex, long> active_eh_;
	struct timeval timeout_;
	//HTTP_EH_MODE http_eh_mode_;
};

#endif /* __HTTP_SOCKET_H__ */
