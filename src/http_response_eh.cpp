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
	//�ͷŽṹ������ǻ�����󣬹黹�������new���󣬻�delete
	this->cleanup();
	return 0;
}

int http_response_eh::response( http_eh *eh,http_request *r,bool close_eh )
{
	//�жϿͻ��������Ƿ��ڣ�����ҵ�������������,�ⲿ�ͷ�r�ṹ

	if (eh->client_active()){
		//���߳�֪ͨ������handle_exception
		this->eh_ = eh;
		this->close_eh_ = close_eh;
		this->request_ = r;
		return eh->reactor()->notify_eh(this);
	}

	return -1;
}

