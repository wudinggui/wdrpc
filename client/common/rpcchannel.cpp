#include "rpcchannel.hpp"
#include "spdlog/spdlog.h"

namespace rpc 
{

Rpcchannel::Rpcchannel()
	:m_time(DEFAULTTIMEOUT)
	,m_callid(0) 
{
}

void Rpcchannel::SetConnection(const Connection_ptr& conn)
{
    m_conn = conn;
}

int32_t Rpcchannel::Call(std::string& name,  std::string& req, std::string& res)
{
    if (!m_conn)
    {
        spdlog::get("console")->info("call function %s fail, connection not found", name.c_str());
        return RPCNOCONN;
    }
	
	spdlog::get("console")->info("call function %s", name.c_str());

	uint32_t id = m_callid++;
	size_t hash = std::hash<std::string>{}(name);
	uint32_t setlength = req.size();
	Message::Header  head(Message::REQUEST, setlength, id, hash);
	Message  msg(head, (char*)req.c_str(), req.size());
    m_conn->Write(msg);

    return GetResponse(id, res);
}

int32_t Rpcchannel::GetResponse(uint32_t callid, std::string& res)
{
    std::promise<std::string> prom;
    std::future<std::string>  fut = prom.get_future(); //future����.
	m_prommap[callid] = std::move(prom);
	
	std::chrono::milliseconds timeout(m_time);
    while (fut.wait_for(timeout) == std::future_status::timeout)
    {
        spdlog::get("console")->info("get response fail, msg id %u, timeout", callid);
        return RPCTIMEOUT;
    }

    res = fut.get();

    spdlog::get("console")->info("get response msg, id %u", callid);

    auto itr = m_prommap.find(callid);
	if (itr != m_prommap.end())
	{
	    m_prommap.erase(itr);
	}
	
	return RPCOK;
}

void Rpcchannel::SetTimeout(uint32_t time)
{
    m_time = time;
}

void Rpcchannel::SetResponse(Message& msg)
{
    auto itr = m_prommap.find(msg.id());
	if (itr == m_prommap.end())
	{
	    spdlog::get("console")->info("set response msg fail, id %u", msg.id());
	    return;
	}

	auto& prom = itr->second;

	prom.set_value(std::string(msg.body(), msg.bodylen()));
}

}

