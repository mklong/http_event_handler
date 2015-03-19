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
	
	//��eventloop�߳�eh�ĺ����е���response��û�п��߳�
	void response_st(http_request *r,bool close_eh);

	//���߳�֪ͨ
	int response_mt(http_request *r,bool close_eh);
	
	//���߳�֪ͨ��eventloop�̺߳�������eh������Ӧ����
	void on_response_mt(http_request *r,bool close_eh);

	//�ڿ���˳�ʱ�����߳�ǿ�йر�
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
	//������󱻶�θ��ã���Ҫʹ�õ���������
	//����ͨ���Աȸñ������ж϶����Ƿ��Ѿ�����
	long used_;

	//eh�Ƿ���ͷţ��ڶ��߳�����£����ڿ��߳�֪ͨ��
	//���ref_��С���㣬����ͻ��˹رգ�Ҳ�����ͷ�
	ACE_Atomic_Op<ACE_Thread_Mutex, long> ref_;
	bool close_eh_;
	bool client_active_;
	
	struct timeval tv_;
	short state_;
	long last_time_;

	//
	http_socket * socket_base_;

	//�ڲ��������ڴ�أ��ٷ��������С�ڴ涼���ڴ�ط���
	http_request * request_;

	//link
	http_request * recv_request_;
	//link 
	http_request * send_request_;

};


#endif /* __HTTP_EH_H__ */
