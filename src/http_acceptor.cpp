#include "http_acceptor.h"
#include "log/srlog.h"

http_acceptor::http_acceptor()
	:max_fds_((size_t)-1),
	socket_base_(NULL)
{

}

http_acceptor::~http_acceptor()
{

}

void http_acceptor::max_fds( size_t size )
{
	max_fds_ = size;
}

int http_acceptor::handle_input( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{
	srlog_tracer(http_acceptor::handle_input);

	int ret = 0;
	int loop = 0;
	ACE_HANDLE s;
	while(1)
	{
		loop ++;
		ACE_INET_Addr remote_addr;
		int len = remote_addr.get_addr_size();

		s = ACE_OS::accept(fd,(sockaddr*)remote_addr.get_addr(),&len);	
		if (s == ACE_INVALID_HANDLE){
			int err = errno;
			if (err == EWOULDBLOCK){
				break;
			}
			else if (err == EMFILE || err ==ENFILE){
				//too many open files
				//stop accept for a while
				srlog_warn("http_acceptor::handle_input | too many open files.");
				break;
			}
			else
			{
				//other errors
				srlog_warn("http_acceptor::handle_input | accept errno : %d.",err);
				break;
			}

		}	

		//统计接收的连接数
		//accept_count_ ++;

		//检查是否已经达到了最大连接数
		long cur_size = socket_base_->active_eh_.value();
		if (cur_size >= max_fds_){
			//与其放在队列中不接收，不如直接close，避免客户端显示超时，导致问题难以定位
			srlog_warn("accept_handler::handle_input | current active fd: %ld ,max fds: %ld.",cur_size,max_fds_);
			ACE_OS::closesocket(s);

			//统计被关闭的连接数

			continue;
		}

		//set socket opt
		int val;
		ACE::record_and_set_non_blocking_mode (s,val);

		if(sndbuf_ != nfl::accept_cfg::UNSET_VALUE){
			ret = ACE_OS::setsockopt(s,SOL_SOCKET,SO_SNDBUF,
				(const char *)&sndbuf_,sizeof(int));
		}

		if(rcvbuf_ != nfl::accept_cfg::UNSET_VALUE){
			ret = ACE_OS::setsockopt(s,SOL_SOCKET,SO_RCVBUF,
				(const char *)&rcvbuf_,sizeof(int));
		}

		if (keepalive_){
			int keepalive = 1;
			ret = ACE_OS::setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,
				(const char *)&keepalive,sizeof(int));
		}

		//eh 在close中调用eh->cleanup会把对象还给cq_eh_
		http_eh * eh = cq_eh_.cqi_new();
		if (eh == NULL){
			//error
			srlog_warn("accept_handler::handle_input | cqi_new failed.");
			ACE_OS::closesocket(s);
			return 0;
		}

		srlog_dbg("http_acceptor::handle_input | recv fd : %d ,current active fd %ld.",s,socket_base_->active_eh_.value());

		eh->handle(s);
		eh->set_http_socket(socket_base_);
		eh->remote_addr(&remote_addr);

		//统计当前活跃连接数
		socket_base_->active_eh_ ++;

		if (dispatcher_){
			ret = dispatcher_(eh,arg_);
			if (ret){
				srlog_warn("accept_handler::handle_input | dispatcher failed.");
			}
		}else{
			eh->reactor(this->reactor());
			ret = eh->open();
			if (ret !=0){
				eh->close();
			}
		}	
	}

	return 0;
}

void http_acceptor::set_http_socket( http_socket* p )
{
	socket_base_ = p;
}


