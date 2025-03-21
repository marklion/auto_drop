#if !defined(_AD_ACTION_H_)
#define _AD_ACTION_H_

#include "../../public/event_sc/ad_redis.h"

typedef std::shared_ptr<AD_REDIS_EVENT_NODE> AD_REDIS_EVENT_NODE_PTR;

AD_REDIS_EVENT_NODE_PTR create_redis_event_node();

#endif // _AD_ACTION_H_
