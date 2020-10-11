//
// Created by System Administrator on 2020/10/11.
//

#include "TLIOContext.h"

static thread_local boost::asio::io_context ctxThreadLocal;

boost::asio::io_context &st::utils::asio::TLIOContext::getIOContext() {
    return ctxThreadLocal;
}
