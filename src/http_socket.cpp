#include "http_socket.h"
#include "core/common/busin_config.h"
#include "log/srlog.h"

http_socket::http_socket()
	:acceptor_(NULL)
{
}

http_socket::~http_socket()
{
}

int http_socket::open( short port , size_t worker_threads, size_t timeout, ACE_HANDLE acpt_hdl /*=0*/,
	bool keep_alive /*= false*/ ,size_t max_fd /*= nfl_os::MaxFd*/)
{
	srlog_tracer(http_socket::open);
	int ret = 0;
	int thread_num = Busin_Conf_Instance()->get_int_val("socket","threads",worker_threads);

	//初始化网络线程池
	ret = threadpool_.open(thread_num);
	if (ret){
		srlog_error("http_socket::init | init socket threadpool failed");
		return ret;
	};

	nfl::accept_cfg cfg;
	nfl::libevent_reactor * accept_reactor = threadpool_.get_accept_reactor();
	if (accept_reactor == NULL){
		srlog_error("http_socket::init | threadpool get_accept_reactor failed.");
		return -1;
	}

	acceptor_ = new http_acceptor;
	//万一下面的逻辑失败，close函数中不会因为未初始化reactor而崩溃
	acceptor_->reactor(accept_reactor);
	acceptor_->set_http_socket(this);

	//初始化客户端连接端口等
	int port_num = Busin_Conf_Instance()->get_int_val("socket","port",port);
	int backlog = Busin_Conf_Instance()->get_int_val("socket","backlog",10240);
	int sndbuf = Busin_Conf_Instance()->get_int_val("socket","sndbuf",nfl::accept_cfg::UNSET_VALUE);
	int rcvbuf = Busin_Conf_Instance()->get_int_val("socket","rcvbuf",nfl::accept_cfg::UNSET_VALUE);

	cfg.backlog = backlog;
	cfg.deferred_accept = false;
	cfg.sndbuf = sndbuf;
	cfg.rcvbuf = rcvbuf;

	if (keep_alive){
		cfg.keepalive = true;
	}

	//从父进程继承的fd，fd = acpt_hdl
	if (port == 0){
		cfg.listen_fd = acpt_hdl;
	}else{
		ret = cfg.addr.set (port_num, (ACE_UINT32)INADDR_ANY);
		if (ret ){
			srlog_error("http_socket::init | set addr failed,port = %d",port_num);
			return -1;
		}
	}
	
	int read_timeout = Busin_Conf_Instance()->get_int_val("socket","read_timeout",timeout);
	timeout_.tv_sec = read_timeout;
	timeout_.tv_usec = 0;

	srlog_crit("http_socket::init | set socket read timeout %d seconds.",read_timeout);

	//设置最大连接数
	int max = Busin_Conf_Instance()->get_int_val("socket","max_fd",max_fd);
	srlog_crit("http_socket::init | max fd is %d  ,current system limit %d",max,nfl_os::MaxFd);
	acceptor_->max_fds(max);

	//初始化连接管理器
	ret = eh_mgr_.open(max);
	if (ret){
		srlog_error("http_socket::init | open eh manager failed. ");
		return -1;
	}

	//绑定端口等
	ret = acceptor_->open(&cfg);
	if (ret){
		srlog_error("http_socket::init | open acceptor failed,port = %d",port_num);
		return -1;
	}

	//设置分派函数
	acceptor_->set_dispatcher(threadpool_.get_dispatcher(),&threadpool_);

	//注册到reactor
	ret = accept_reactor->add_eh(static_cast<nfl::event_handler*>(acceptor_));
	if (ret){
		srlog_error("http_socket::init | add acceptor event failed,port : %d.",port_num);
		return ret;
	}

	srlog_crit("http_socket::init | open port : %d.",port_num);

	return 0;
}

int http_socket::close()
{
	int ret = 0;

	if (acceptor_ != NULL){
		if (acceptor_->active()){
			ret = acceptor_->reactor()->notify_eh(acceptor_,nfl::CLOSE_MASK);
		}

		//确保已经关闭fd并从reactor中移除
		while(acceptor_->active()){
			ACE_OS::sleep(1);
		}

		//友好的等待正在使用的连接关闭
		for(int i = 0; i < 20; i++){	
			if (active_eh_.value() == 0){
				break;
			}
			ACE_OS::sleep(1);
		}

		//暴力关闭
		if (active_eh_.value() != 0){
			//通知所有eh强行关闭
			for (int i = 0;i < eh_mgr_.max_size(); i++){
				http_eh * eh = eh_mgr_.find_eh(i);
				if (eh == NULL){
					continue;
				}

				eh->close_mt();
			}
		}
	}

	threadpool_.close();
	eh_mgr_.close();

	if (acceptor_ != NULL){
		acceptor_->close();
		delete acceptor_;
		acceptor_ = NULL;
	}

	return 0;
}

// void http_socket::set_http_mode( HTTP_EH_MODE m )
// {
// 	http_eh_mode_ = m;
// }

void http_socket::on_task( http::http_request *r ,http_eh *eh )
{
	return ;
}
