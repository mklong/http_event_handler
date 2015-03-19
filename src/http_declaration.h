/** 
 * @file	http_declaration.h
 * @brief
 *		httpЭ����
 *		1.��http_eh����http_request,����Ͷ�ݵ�http_socket���鷽������
 *		ͬʱ�ṩͬ�̺߳Ϳ��̵߳���Ӧ����
 *		2.����http_requestʹ����һ���ڴ�أ��ڴ����ǳ�����
 *		3.���֧��pipelineģʽ��֧�ֲ���Ӧ
 *		
 * detail...
 * 
 * @author	mklong
 * @version	1.0
 * @date	2014/12/17
 * 
 * @see		
 * 
 * <b>History:</b><br>
 * <table>
 *  <tr> <th>Version	<th>Date		<th>Author	<th>Notes</tr>
 *  <tr> <td>1.0		<td>2014/12/17	<td>mklong	<td>Create this file</tr>
 * </table>
 * 
 */
#ifndef __HTTP_DECLARATION_H__
#define __HTTP_DECLARATION_H__

class http_eh;
class http_response_eh;
class http_acceptor;
class http_socket;


#include "http_eh.h"
#include "http_response_eh.h"
#include "http_acceptor.h"
#include "http_socket.h"


#endif /* __HTTP_DECLARATION_H__ */
