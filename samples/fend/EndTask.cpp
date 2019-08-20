/// \file     EndTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <config.hpp>
#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "EndTask.hpp"
#include "Logger.hpp"
#include "protocol.h"

EndTask::EndTask(boost::asio::local::stream_protocol::socket& socket,
                                                     darwin::Manager& manager,
                                                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                                     std::string redis_socket_path)
        : Session{socket, manager, cache}, _redis_manager{redis_socket_path}{
    _is_cache = _cache != nullptr;
    _redis_manager.ConnectToRedis();
}

void EndTask::operator()() {
    std::string evt_id, nb_result;

    evt_id = Evt_idToString();
    nb_result = std::to_string(header.certitude_size);
    REDISAdd(evt_id, nb_result);
    Workflow();
}

long EndTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_END;
}

void EndTask::Workflow() {
    switch (header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendToDarwin();
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
        default:
            break;
    }
}

bool EndTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("EndTask:: ParseBody: " + body);
    return true;
}

bool EndTask::REDISAdd(const std::string& evt_id, const std::string& nb_result) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("EndTask::REDISAdd:: Add to key 'darwin_<" +  evt_id + ">' the number :" + nb_result);

    std::vector<std::string> arguments;
    arguments.emplace_back("SET");
    arguments.emplace_back("darwin_" + evt_id);
    arguments.emplace_back(nb_result);

    redisReply *reply = nullptr;
    reply = nullptr;

    if (!_redis_manager.REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("EndTask::REDISAdd:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        return false;
    }

    if (!reply || reply->type != REDIS_REPLY_STATUS) {
        DARWIN_LOG_ERROR("EndTask::REDISAdd:: Not the expected Redis response ");
        freeReplyObject(reply);
        return false;
    }

    std::string status = reply->str;
    DARWIN_LOG_DEBUG("EndTask::REDISAdd:: Status for deleting processed logs : " + status);
    freeReplyObject(reply);
    reply = nullptr;

    return true;
}