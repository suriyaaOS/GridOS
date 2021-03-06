/**
	The Grid Core Library
 */

/**
	Message slots
 */

#include <types.h>

#include "message.h"

/**
	@brief 不定长度消息的状态初始化
 
	@return
		The next slots after count
 */
static struct y_message *message_clean(struct y_message_instance *instance, struct y_message *start, int count)
{
	struct y_message * cur = start;
	count = __add_msg_count__(count);
	
	/* 把数据区所占的slot 的flag 清掉，所以循环的是 msg count */
	while (count > 0)
	{
		cur->flags = 0;
		MSG_GET_NEXT_SLOT(instance, cur);
		count --;
	}

	return cur;
}

/**
	@brief 处理消息
 
	如果消息是同步的，则要调用内核去响应，因为发送者在等待我们的回应。
	如果消息是异步的，则直接清掉占用位即可。
 
	@return
		The next postion to fetch the message.The message length is dynmaic!
 */
static struct y_message *handle_message(struct y_message_instance *instance, struct y_message *what)
{
	int count;
	int r = true;
	struct y_message * next;
	
	if (instance->filter) r = instance->filter(what);
	count = what->count;										//record the src count before handling message, sync msg will overwrite it.
	
	if (r == true)
	{
		
		/*
		 1，地址调用型的？
		 2，那么就是查询的
		 */
		if (what->flags & MSG_FLAGS_ADDRESS)
		{
			y_message_func fn;
			fn = (y_message_func)what->what;
			fn(what);
		}
		else
		{
			y_message_func fn;
			if (instance->find_handler)
			{
				fn = instance->find_handler(instance, what->what);
				if (fn)
					fn(what);
			}
		}
	}
	
	/*
		ACK the sender if it's a synchronouse message
	 */
	if (what->flags & MSG_FLAGS_SYNC)
	{
		/*
		 The sender is waiting in kernel,
		 so let kernel know the message has bean handled and
		 let the sender copy back the result.
		 When these are all done, we can safely clean the message.
		 */
		instance->response_sync(what);
	}
	
	/*
		Clear the message.
		The head is cleaned when the data part has been cleaned.
	 */
	next = what;
	MSG_GET_NEXT_SLOT(instance, next);
	next = message_clean(instance, next, count);
	what->flags = 0;

	return next;
}

/**
	@brief 将所有消息的状态初始化
 */
void message_reset_all(struct y_message_instance *instance)
{
	struct y_message *cur = instance->slots;
	
	while(1)
	{
		cur->flags = 0;
		MSG_GET_NEXT_SLOT(instance, cur);
		if (cur == instance->slots)
			break;
	}
}

/**
	@brief 等待一个消息，并返回
 */
void message_loop(struct y_message_instance *instance)
{
	struct y_message *cur;

	instance->sleep(instance);
	cur = instance->slots;

	while(1)
	{
		/*
			Check for first loop
		 */
		while (cur->flags & MSG_STATUS_NEW)
			cur = handle_message(instance, cur);

		/*
			目前,如果没有消息,休眠第一次可能不成功,因为可能有wakeup计数器。那么本次要去清除它。
			因此,程序马上回到上面检查CUR的状态,如果为空紧接着是第二次休眠一般是成功的.
		 */
		instance->sleep(instance);
	}
}
