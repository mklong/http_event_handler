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
// 		HTTP_PIPELINE = 0,		/* ������Ӧһһ��Ӧ������Ƕ��������Ҫ�ŶӴ��� */
// 		HTTP_STREAM,				/* ������Ӧ�����Ӧ������ֻ������û����Ӧ */
// 	};

public:
	http_socket();
	~http_socket();

	//���ù���ģʽ����open֮ǰ����
	//void set_http_mode(HTTP_EH_MODE m);

	//�������ݴ���ص�
	

	//���������̳߳��Լ�listen socket
	int open(short port , size_t worker_threads, size_t timeout, 
		ACE_HANDLE acpt_hdl  =0, bool keep_alive = false,size_t max_fd = nfl_os::MaxFd);

	//ֹͣ����ģ��
	int close();

	//
	virtual void on_task(http::http_request *r ,http_eh *eh);

protected:

	//���ڼ�¼���߲�ѯeh��״̬���ݲ�ʹ��
	eh_state_mgr<http_eh> eh_mgr_;

	//���߳�֪ͨ�¼�����
	nfl::cq_base<http_response_eh> resp_eh_cache_;


	//���������ģ����ǰ��������Ч��ʹ��ָ�����
	http_acceptor *acceptor_;
	nfl::sock_thread_pool threadpool_;

	ACE_Atomic_Op<ACE_Thread_Mutex, long> active_eh_;
	struct timeval timeout_;
	//HTTP_EH_MODE http_eh_mode_;
};

#endif /* __HTTP_SOCKET_H__ */
