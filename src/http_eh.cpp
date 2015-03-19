#include "http_eh.h"
#include "log/srlog.h"
#include "core/nfl/libevent_reactor.h"
#include "openlib/nginx/libngx.h"


http_eh::http_eh():
used_(0)
{

}

int http_eh::open()
{
	srlog_tracer(http_eh::open);
	
	ref_ = 0;
	close_eh_ = false;
	client_active_ = true;

	state_ = WAITING;
	request_ = NULL;
	send_request_ = NULL;
	recv_request_ = NULL;

	socket_base_->eh_mgr_.add(this);

	struct timeval t;
	event_base_gettimeofday_cached(this->reactor()->base(),&t);
	last_time_ = t.tv_sec;

	tv_ = socket_base_->timeout_;


	return eh_add(nfl::READ_MASK | nfl::PERSIST_MASK,&tv_);
}

int http_eh::close()
{
	srlog_tracer(http_eh::close);

	if (request_){
		http_request::finalize_http_request(request_);
		request_ = NULL;
	}

	while (send_request_){
		http_request *r = send_request_;
		send_request_ = send_request_->next;
		http_request::finalize_http_request(r);
	}

	while (recv_request_){
		http_request *r = recv_request_;
		recv_request_ = recv_request_->next;
		http_request::finalize_http_request(r);
	}

	//递减活跃数
	socket_base_->active_eh_ --;

	//递增used_,超过归零
	if (++ used_ >100000 ){
		used_ = 0;
	}

	socket_base_->eh_mgr_.remove(this);

	return nfl::event_handler::close();
}

int http_eh::handle_input( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{
	srlog_tracer(http_eh::handle_input);
	int ret = 0;

	if ( ACE_BIT_ENABLED(state_,WAITING) ){
		
		//由等待状态改为正在读状态
		ACE_CLR_BITS(state_,WAITING);
		ACE_SET_BITS(state_,READING);

		//协议数据，检查是否之前由于出现粘包已经分配过接收缓冲区，没有的话
		//需要先分配
		if (request_ == NULL){

			//创建request结构体,本身从内存池分配，也附带在内存池上
			request_ = http_request::create_http_request();
			if (request_ == NULL){
				srlog_error("http_eh::handle_input | create_http_request failed");
				return -1;
			}
			//解析数据
		
			ret = request_->init_request_parser(http_request::HTTP_BOTH);
			if (ret ){
				srlog_error("http_eh::handle_input | init_request_parser failed");
				return -1;
			}

			srlog_dbg("http_eh::handle_input | create new http_request :%p",request_);
		}

		return on_data(fd);

	}else if (ACE_BIT_ENABLED(state_,READING)){
		//之前已经接受过数据，但是未接收完整

		return on_data(fd);
	}

	srlog_error("http_eh::handle_input | ukown state : %d. ",state_ );
	//unkown
	return -1;
}

int http_eh::handle_output( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{
	srlog_tracer(http_eh::handle_output);

	http_request * r = send_request_;
	while(r){

		//开始发送数据
		ngx_chain_t *cur_chain_ = &r->send_chain;
		while(cur_chain_){
			ngx_buf_t * b = cur_chain_->buf;
			if(b == NULL){
				//无数据
				cur_chain_ = cur_chain_->next;
				continue;
			}

			size_t len = ngx_buf_size(b);
			if (len == 0){
				//已经发送过
				cur_chain_ = cur_chain_->next;
				continue;
			}

			//每次最多发送4K
			if (len > 4096)
				len = 4096;

			int n = ACE_OS::send(fd,(char *)b->pos,len,0);

			if (n == 0){
				srlog_dbg("http_eh::handle_output | event server closed socket");
				client_active_ = false;
				return -1;
			}else if (n == -1 ){
				int err = errno;
				if (err == EWOULDBLOCK){

					//没注册过写事件，先注册
					if (ACE_BIT_DISABLED(state_,WRITING)){
						eh_wakeup_mask(nfl::WRITE_MASK,&this->tv_);
					}

					srlog_dbg("http_eh::handle_output | send buffer EWOULDBLOCK,add event now.");
					return 0;
				}
				srlog_dbg("http_eh::handle_output | send error ,errno:%d",err);
				client_active_ = false;
				return -1;
			}

			b->pos += n;
			if (b->pos != b->last){
				continue;
			}
			else{
				cur_chain_ = cur_chain_->next;
			}	
		}

		//发送完request后,继续发送
		http_request * old = r;
		r = r->next;
		send_request_ = r; 
		http_request::finalize_http_request(old);
		srlog_info("http_eh::handle_output | send request successfully :%p.",old);
	}

	//取消写事件
	if (ACE_BIT_ENABLED(state_,WRITING)){
		ACE_CLR_BITS(state_,WRITING);
		eh_cancel_mask(nfl::WRITE_MASK,&this->tv_);
	}else{
		//只更新定时器
		eh_timer_update(&this->tv_);
	}

	if (close_eh_){
		srlog_dbg("http_eh::handle_output | close event handler after send request.");
		return -1;
	}
	
	return 0;
}

int http_eh::handle_timeout( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{
	srlog_tracer(http_eh::handle_timeout);

	char c;
	int n = ACE_OS::recv(fd,&c,1,MSG_PEEK);
	int err = errno;

	struct timeval t;
	event_base_gettimeofday_cached(this->reactor()->base(),&t);

	char buf[32];
	this->remote_addr()->addr_to_string(buf,32);
	srlog_warn("http_eh::handle_timeout | peer addr : %s,"
		"last time: %ld,current time: %ld ,try recv : %d, errno : %d.",buf,last_time_,t.tv_sec,
		n,err);
	return -1;
}

int http_eh::handle_exception( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{
	srlog_tracer(http_eh::handle_exception);

	//上层通知，要么是通知关闭，要么是需要抛任务或者触发写
	ref_ --;

	if( !close_eh_ && client_active_){
		//后端不要求关闭，检查是否有数据继续后抛
		//写逻辑全部从add_request走
		return this->throw_check();
	}

	return -1;
}

int http_eh::handle_close( ACE_HANDLE fd,short close_mask )
{
	srlog_tracer(http_eh::handle_close);
	
	//当前还有数据在写
	if (client_active_ && ACE_BIT_ENABLED(state_,WRITING) ){
		srlog_dbg("http_eh::handle_close | sending response,waitting.");
		return 0;
	}

	//取消事件
	if (this->active()){
		client_active_ = false;
		nfl::event_handler::handle_close(fd,close_mask);
	}

	//eh结构尚不能释放
	if (ref_.value() > 0){
		srlog_dbg("http_eh::handle_close | ref is not 0,waitting.");
		return 0;
	}
	
	//上层无需使用，可以释放
	return http_eh::close();
}

void http_eh::response_st( http_request *r,bool close_eh )
{
	srlog_tracer(http_eh::response_st);
	//递减引用
	ref_ --;

	//如果已经被设置关闭，则忽略该值
	if (!close_eh_){
		close_eh_ = close_eh;
	}

	//设置发送数据
	if (r != NULL){
		if (send_request_ == NULL){
			send_request_ = r;
			r->next = NULL;
		}else{
			http_request * last = send_request_;
			while(last->next != NULL)
				last = last->next;

			last->next = r;
			r->next = NULL;
		}
	}
}

int http_eh::response_mt( http_request *r,bool close_eh )
{
	srlog_tracer(http_eh::response_mt);
	if ( r != NULL){
		http_response_eh *resp_eh = this->socket_base_->resp_eh_cache_.cqi_new();
		//跨线程响应
		int ret = resp_eh->response(this,r,close_eh);
		if (ret){
			resp_eh->cleanup();
			return -1;
		}
		return 0;
	}else{
		//只需要通知即可,触发handle_exception
		this->close_eh_ = close_eh;
		return this->reactor()->notify_eh(this);
	}
	
}

void http_eh::on_response_mt( http::http_request *r ,bool close_eh)
{
	srlog_tracer(http_eh::on_response_mt);
	//递减引用
	ref_ --;

	add_send_request(r);

	//如果已经被设置关闭，则忽略该值
	if (!close_eh_){
		close_eh_ = close_eh;
	}
	
	//先调用写
	int ret = this->handle_output(this->handle());
	if (ret){
		this->handle_close(this->handle(),nfl::CLOSE_MASK );
		return;
	}
	
	//检查是否需要投递任务
	if( !close_eh_ && client_active_){
		ret = throw_check();
		if (ret){
			this->handle_close(this->handle(),nfl::CLOSE_MASK);
			return;
		}
	}
}

long http_eh::used()
{
	return used_;
}

int http_eh::on_data( ACE_HANDLE fd )
{
	srlog_tracer(channel_eh::on_data);

	int n = 0;
	int ret = 0;
	ngx_buf_t *b;
	size_t rest_len = 0;
	bool need_loop;

	while(1){

		need_loop = false;

		//数据接收buf，初始化为页大小，通常够接收一个完整的客户端数据包
		b = request_->buf;	

		//buf剩余大小
		rest_len =  b->end - b->last;
		if (rest_len == 0){
			//
			request_->alloc_recv_buf();
			b = request_->buf;
			rest_len =  b->end - b->last;
			srlog_dbg("http_eh::on_data | alloc new recv buffer");
		}

		//是否已经有未解析的数据，pos为读指针，last为写指针
		n = ngx_buf_size(b);
		if (n == 0)
		{
			n = ACE_OS::recv(fd,(char *)b->last,rest_len,0);

			if (n == 0){
				srlog_dbg("http_eh::on_data | client closed socket");
				client_active_ = false;
				return -1;
			}else if (n == -1 ){
				int err = errno;
				if (err == EWOULDBLOCK){
					srlog_dbg("http_eh::on_data | recv buffer wouldblock");
					return 0;
				}
				srlog_error("http_eh::on_data | recv buffer error,errno:%d",err);
				client_active_ = false;
				return -1;
			}

			//写指针后移
			b->last += n; 
			need_loop = (n == rest_len);
			srlog_dbg("http_eh::on_data | recv buffer size: %d,buffer rest size: %d",n,rest_len);
		}

		bool done = false;

		int nparsed = request_->run_http_parser((const char*)b->pos,n,&done);
		if (nparsed <0){
			//后续优化数据中存在%号导致崩溃的问题
			srlog_warn("http_eh::on_data | run_http_parser failed,error:%s ,data:%.*s",
				request_->parser_errno(),n,b->pos);
			return -1;
		}

		//解析过的数据，读指针后移
		b->pos += nparsed;

		if (done){
			srlog_dbg("http_eh::on_data | run_http_parser done");
			http_request * deal_request = request_;

			//多余的部分属于粘包，重新生成request结构，并拷贝这部分数据
			//连续发送大量小包的情况下，此处容易出现多次拷贝的情况
			//后续可以考虑优化

			if (nparsed < n){
				size_t more = n - nparsed;

				http_request * new_request = http_request::create_http_request();
				new_request->init_request_parser(http_request::HTTP_BOTH);
				ngx_memcpy(new_request->buf->pos,b->pos,more);

				srlog_dbg("http_eh::on_data | copy buf to new request : %p , size ： %d.",new_request,more);

				new_request->buf->last += more;

				//将last提前
				b->last = b->pos;

				request_ = new_request;

				//需要循环解析
				need_loop = true;

				srlog_dbg("http_eh::on_data | run_http_parser done,and create new http request");

			}else{
				request_ = NULL;
			}

			//处理deal_request
			ret = process_request(deal_request);
			srlog_dbg("http_eh::on_data | deal request done");
			if (ret){
				return ret;
			}
		}

		//还有更多的数据
		if (need_loop){
			srlog_dbg("http_eh::on_data | coutinue recv or parse data");
			continue;

		}else{
			if (request_ == NULL){

				//由读状态改为等待状态
				ACE_CLR_BITS(state_,READING);
				ACE_SET_BITS(state_,WAITING);			
				srlog_dbg("http_eh::on_data | finish request");
			}
			eh_timer_update(&this->tv_);
			return 0;
		}
	}
}

int http_eh::process_request( http_request *r )
{
	int ret = 0;
	add_recv_request(r);

	ret = throw_check();
	if (ret ){
		return ret;
	}
	return 0;
}

void http_eh::set_http_socket( http_socket * p )
{
	socket_base_ = p;
}

int http_eh::throw_check()
{
	srlog_tracer(http_eh::throw_check);

	//暂不抛任务
	if (ref_.value() > 0){
		return 0;
	}

	while(1){

		http_request *r = get_recv_request();
		if (r == NULL){
			return 0;
		}

		//递增，在response等函数中递减
		ref_ ++;
		socket_base_->on_task(r,this);

		//检查状态

		//数据可能是跨线程处理
		if (ref_.value() > 0){
			return 0;
		}

		//有数据需要发送
		if (send_request_ != NULL && ACE_BIT_DISABLED(state_,WRITING)){
			int ret = handle_output(this->handle());
			if (ret){
				return ret;
			}
		}

		//没有数据发送，走关闭逻辑
		if (close_eh_){
			return -1;
		}
		//不关闭，继续投递数据
	}
}

void http_eh::add_recv_request( http_request *r )
{
	if (recv_request_ == NULL){
		recv_request_ = r;
		
	}else{
		//挂在末尾
		http_request * req = recv_request_;
		while(req->next != NULL)
			req = req->next;

		req->next = r;
	}
}

http_request * http_eh::get_recv_request()
{
	http_request *r = recv_request_;
	if (r == NULL){
		return NULL;
	}
	
	//head out
	recv_request_ = r->next;
	//置空，否则response时会是个链状结构
	r->next = NULL;
	return r;
}

bool http_eh::client_active()
{
	return client_active_;
}

void http_eh::add_send_request( http_request *r )
{

	if (send_request_ == NULL){
		send_request_ = r;

	}else{
		//挂在末尾
		http_request * req = send_request_;
		while(req->next != NULL)
			req = req->next;

		req->next = r;
	}
}

void http_eh::close_mt()
{
	//当前业务线程尚未响应
	
	this->ref_ ++;
	close_eh_ = true;
	this->reactor()->notify_eh(this);
}






