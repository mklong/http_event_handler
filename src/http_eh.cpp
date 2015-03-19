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

	//�ݼ���Ծ��
	socket_base_->active_eh_ --;

	//����used_,��������
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
		
		//�ɵȴ�״̬��Ϊ���ڶ�״̬
		ACE_CLR_BITS(state_,WAITING);
		ACE_SET_BITS(state_,READING);

		//Э�����ݣ�����Ƿ�֮ǰ���ڳ���ճ���Ѿ���������ջ�������û�еĻ�
		//��Ҫ�ȷ���
		if (request_ == NULL){

			//����request�ṹ��,������ڴ�ط��䣬Ҳ�������ڴ����
			request_ = http_request::create_http_request();
			if (request_ == NULL){
				srlog_error("http_eh::handle_input | create_http_request failed");
				return -1;
			}
			//��������
		
			ret = request_->init_request_parser(http_request::HTTP_BOTH);
			if (ret ){
				srlog_error("http_eh::handle_input | init_request_parser failed");
				return -1;
			}

			srlog_dbg("http_eh::handle_input | create new http_request :%p",request_);
		}

		return on_data(fd);

	}else if (ACE_BIT_ENABLED(state_,READING)){
		//֮ǰ�Ѿ����ܹ����ݣ�����δ��������

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

		//��ʼ��������
		ngx_chain_t *cur_chain_ = &r->send_chain;
		while(cur_chain_){
			ngx_buf_t * b = cur_chain_->buf;
			if(b == NULL){
				//������
				cur_chain_ = cur_chain_->next;
				continue;
			}

			size_t len = ngx_buf_size(b);
			if (len == 0){
				//�Ѿ����͹�
				cur_chain_ = cur_chain_->next;
				continue;
			}

			//ÿ����෢��4K
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

					//ûע���д�¼�����ע��
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

		//������request��,��������
		http_request * old = r;
		r = r->next;
		send_request_ = r; 
		http_request::finalize_http_request(old);
		srlog_info("http_eh::handle_output | send request successfully :%p.",old);
	}

	//ȡ��д�¼�
	if (ACE_BIT_ENABLED(state_,WRITING)){
		ACE_CLR_BITS(state_,WRITING);
		eh_cancel_mask(nfl::WRITE_MASK,&this->tv_);
	}else{
		//ֻ���¶�ʱ��
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

	//�ϲ�֪ͨ��Ҫô��֪ͨ�رգ�Ҫô����Ҫ��������ߴ���д
	ref_ --;

	if( !close_eh_ && client_active_){
		//��˲�Ҫ��رգ�����Ƿ������ݼ�������
		//д�߼�ȫ����add_request��
		return this->throw_check();
	}

	return -1;
}

int http_eh::handle_close( ACE_HANDLE fd,short close_mask )
{
	srlog_tracer(http_eh::handle_close);
	
	//��ǰ����������д
	if (client_active_ && ACE_BIT_ENABLED(state_,WRITING) ){
		srlog_dbg("http_eh::handle_close | sending response,waitting.");
		return 0;
	}

	//ȡ���¼�
	if (this->active()){
		client_active_ = false;
		nfl::event_handler::handle_close(fd,close_mask);
	}

	//eh�ṹ�в����ͷ�
	if (ref_.value() > 0){
		srlog_dbg("http_eh::handle_close | ref is not 0,waitting.");
		return 0;
	}
	
	//�ϲ�����ʹ�ã������ͷ�
	return http_eh::close();
}

void http_eh::response_st( http_request *r,bool close_eh )
{
	srlog_tracer(http_eh::response_st);
	//�ݼ�����
	ref_ --;

	//����Ѿ������ùرգ�����Ը�ֵ
	if (!close_eh_){
		close_eh_ = close_eh;
	}

	//���÷�������
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
		//���߳���Ӧ
		int ret = resp_eh->response(this,r,close_eh);
		if (ret){
			resp_eh->cleanup();
			return -1;
		}
		return 0;
	}else{
		//ֻ��Ҫ֪ͨ����,����handle_exception
		this->close_eh_ = close_eh;
		return this->reactor()->notify_eh(this);
	}
	
}

void http_eh::on_response_mt( http::http_request *r ,bool close_eh)
{
	srlog_tracer(http_eh::on_response_mt);
	//�ݼ�����
	ref_ --;

	add_send_request(r);

	//����Ѿ������ùرգ�����Ը�ֵ
	if (!close_eh_){
		close_eh_ = close_eh;
	}
	
	//�ȵ���д
	int ret = this->handle_output(this->handle());
	if (ret){
		this->handle_close(this->handle(),nfl::CLOSE_MASK );
		return;
	}
	
	//����Ƿ���ҪͶ������
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

		//���ݽ���buf����ʼ��Ϊҳ��С��ͨ��������һ�������Ŀͻ������ݰ�
		b = request_->buf;	

		//bufʣ���С
		rest_len =  b->end - b->last;
		if (rest_len == 0){
			//
			request_->alloc_recv_buf();
			b = request_->buf;
			rest_len =  b->end - b->last;
			srlog_dbg("http_eh::on_data | alloc new recv buffer");
		}

		//�Ƿ��Ѿ���δ���������ݣ�posΪ��ָ�룬lastΪдָ��
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

			//дָ�����
			b->last += n; 
			need_loop = (n == rest_len);
			srlog_dbg("http_eh::on_data | recv buffer size: %d,buffer rest size: %d",n,rest_len);
		}

		bool done = false;

		int nparsed = request_->run_http_parser((const char*)b->pos,n,&done);
		if (nparsed <0){
			//�����Ż������д���%�ŵ��±���������
			srlog_warn("http_eh::on_data | run_http_parser failed,error:%s ,data:%.*s",
				request_->parser_errno(),n,b->pos);
			return -1;
		}

		//�����������ݣ���ָ�����
		b->pos += nparsed;

		if (done){
			srlog_dbg("http_eh::on_data | run_http_parser done");
			http_request * deal_request = request_;

			//����Ĳ�������ճ������������request�ṹ���������ⲿ������
			//�������ʹ���С��������£��˴����׳��ֶ�ο��������
			//�������Կ����Ż�

			if (nparsed < n){
				size_t more = n - nparsed;

				http_request * new_request = http_request::create_http_request();
				new_request->init_request_parser(http_request::HTTP_BOTH);
				ngx_memcpy(new_request->buf->pos,b->pos,more);

				srlog_dbg("http_eh::on_data | copy buf to new request : %p , size �� %d.",new_request,more);

				new_request->buf->last += more;

				//��last��ǰ
				b->last = b->pos;

				request_ = new_request;

				//��Ҫѭ������
				need_loop = true;

				srlog_dbg("http_eh::on_data | run_http_parser done,and create new http request");

			}else{
				request_ = NULL;
			}

			//����deal_request
			ret = process_request(deal_request);
			srlog_dbg("http_eh::on_data | deal request done");
			if (ret){
				return ret;
			}
		}

		//���и��������
		if (need_loop){
			srlog_dbg("http_eh::on_data | coutinue recv or parse data");
			continue;

		}else{
			if (request_ == NULL){

				//�ɶ�״̬��Ϊ�ȴ�״̬
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

	//�ݲ�������
	if (ref_.value() > 0){
		return 0;
	}

	while(1){

		http_request *r = get_recv_request();
		if (r == NULL){
			return 0;
		}

		//��������response�Ⱥ����еݼ�
		ref_ ++;
		socket_base_->on_task(r,this);

		//���״̬

		//���ݿ����ǿ��̴߳���
		if (ref_.value() > 0){
			return 0;
		}

		//��������Ҫ����
		if (send_request_ != NULL && ACE_BIT_DISABLED(state_,WRITING)){
			int ret = handle_output(this->handle());
			if (ret){
				return ret;
			}
		}

		//û�����ݷ��ͣ��߹ر��߼�
		if (close_eh_){
			return -1;
		}
		//���رգ�����Ͷ������
	}
}

void http_eh::add_recv_request( http_request *r )
{
	if (recv_request_ == NULL){
		recv_request_ = r;
		
	}else{
		//����ĩβ
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
	//�ÿգ�����responseʱ���Ǹ���״�ṹ
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
		//����ĩβ
		http_request * req = send_request_;
		while(req->next != NULL)
			req = req->next;

		req->next = r;
	}
}

void http_eh::close_mt()
{
	//��ǰҵ���߳���δ��Ӧ
	
	this->ref_ ++;
	close_eh_ = true;
	this->reactor()->notify_eh(this);
}






