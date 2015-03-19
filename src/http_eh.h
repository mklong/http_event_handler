#ifndef __HTTP_EH_H__
#define __HTTP_EH_H__

#include "http_declaration.h"
#include "core/nfl/libevent_handler.h"
#include "core/http/http_request.h"

using namespace http;

class http_eh:public nfl::event_handler
{
	enum STATES{
		NOTHING = 0,
		WAITING = 1, //waiting for socket readable  
		READING = 2, //reading msg
		WRITING = 4, //writing msg
		CLOSING = 8, //close
	};

public:
	http_eh();

	virtual int open();
	virtual int close();
	
	//在eventloop线程eh的函数中调用response，没有垮线程
	void response_st(http_request *r,bool close_eh);

	//跨线程通知
	int response_mt(http_request *r,bool close_eh);
	
	//跨线程通知到eventloop线程后，由其他eh设置响应数据
	void on_response_mt(http_request *r,bool close_eh);

	//在框架退出时，跨线程强行关闭
	void close_mt();
    
	void set_http_socket(http_socket * p);
	
	long used();

	bool client_active();

protected:

	virtual int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);
	virtual int handle_output (ACE_HANDLE fd = ACE_INVALID_HANDLE);
	virtual int handle_timeout (ACE_HANDLE fd = ACE_INVALID_HANDLE);
	virtual int handle_exception (ACE_HANDLE fd = ACE_INVALID_HANDLE);
	virtual int handle_close( ACE_HANDLE fd,short close_mask );

	int on_data(ACE_HANDLE fd);
	int process_request(http_request *r);
	
	int throw_check();

	void add_recv_request(http_request *r );
	http_request * get_recv_request();

	void add_send_request(http_request *r);

private:
	//缓存对象被多次复用，需要使用递增计数，
	//便于通过对比该变量来判断对象是否已经过期
	long used_;

	//eh是否可释放，在多线程情况下，存在跨线程通知，
	//如果ref_不小于零，即便客户端关闭，也不能释放
	ACE_Atomic_Op<ACE_Thread_Mutex, long> ref_;
	bool close_eh_;
	bool client_active_;
	
	struct timeval tv_;
	short state_;
	long last_time_;

	//
	http_socket * socket_base_;

	//内部先生成内存池，再非配出对象，小内存都由内存池分配
	http_request * request_;

	//link
	http_request * recv_request_;
	//link 
	http_request * send_request_;

};


#endif /* __HTTP_EH_H__ */
