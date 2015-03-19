#include "http_response_eh.h"

void http_response_eh::reset()
{
	close_eh_ = false;
	eh_ = NULL;
	request_ = NULL;
}

int http_response_eh::handle_exception( ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/ )
{

	eh_->on_response_mt(request_,close_eh_);
	
	reset();
	//释放结构，如果是缓存对象，归还，如果是new对象，会delete
	this->cleanup();
	return 0;
}

int http_response_eh::response( http_eh *eh,http_request *r,bool close_eh )
{
	//判断客户端连接是否还在，否则业务层消化错误码,外部释放r结构

	if (eh->client_active()){
		//跨线程通知，触发handle_exception
		this->eh_ = eh;
		this->close_eh_ = close_eh;
		this->request_ = r;
		return eh->reactor()->notify_eh(this);
	}

	return -1;
}

