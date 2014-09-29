#include <algorithm>
#include <functional>

#include <rutil/Logger.hxx>

#ifdef check
   #undef check
#endif

#include "msrp/System.hxx"
#include "msrp/ConnectionPool.hxx"

using namespace msrp;
using namespace std;
using namespace boost;
using namespace asio;

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::TRANSPORT

ConnectionPool::ConnectionPool(asio::io_service& ios) :
   mService(ios)
{}

ConnectionPool::~ConnectionPool()
{}

void
ConnectionPool::add(shared_ptr<Connection> c)
{
   mConnects.push_back(c);

   // automatically remove the connection from the pool on permanent disconnect
   c->onDisconnect().connect(bind(&ConnectionPool::onDisconnect, this, c, _1));
}

void
ConnectionPool::release(shared_ptr<Connection> c)
{
   vector<shared_ptr<Connection> >::iterator i =
      std::find(mConnects.begin(), mConnects.end(), c);

   if (i != mConnects.end())
   {
      mConnects.erase(i);
   }
}

template<typename Value>
class CompareConnection :
   public unary_function<shared_ptr<Connection>, bool>
{
   public:
      CompareConnection(const_mem_fun_t<const Value, Connection> f, const Value& value) :
         mGet(f), mValue(value)
      {}

      bool operator()(shared_ptr<Connection> connection) const
      {
         return mGet(connection.get()) == mValue;
      }

   private:
      const_mem_fun_t<const Value, Connection> mGet;

      const Value mValue;
};

shared_ptr<Connection>
ConnectionPool::find(const ip::tcp::endpoint& peer) const
{
   return find_if(CompareConnection<ip::tcp::endpoint>(mem_fun(&Connection::peer), peer));
}

shared_ptr<Connection>
ConnectionPool::find(const ip::address& addr) const
{
   return find_if(CompareConnection<ip::address>(mem_fun(&Connection::address), addr));
}

shared_ptr<Connection>
ConnectionPool::findLocal(const asio::ip::tcp::endpoint& local) const
{
   return find_if(CompareConnection<ip::tcp::endpoint>(mem_fun(&Connection::local), local));
}

bool
ConnectionPool::member(shared_ptr<const Connection> c) const
{
   return std::find(mConnects.begin(), mConnects.end(), c) != mConnects.end();
}

void
ConnectionPool::close()
{
   for (vector<shared_ptr<Connection> >::iterator i = mConnects.begin();
         i != mConnects.end(); ++i)
   {
      shared_ptr<Connection> c(*i);

      if (c->state() != Connection::Disconnected)
      {
         c->close();
      }
   }

   mConnects.clear();
}

void
ConnectionPool::onDisconnect(weak_ptr<Connection> wc, const asio::error_code&)
{
   shared_ptr<Connection> c = wc.lock();

   if (c && c->remainingTargets() == 0)
   {
      // If Connection gets through the chain of disconnection event listeners
      // and is still inactive -- i.e., nobody chose to reconnect and there
      // are no remaining targets -- then remove the connection from the pool.
      mService.post(bind(&ConnectionPool::conditionalRelease, this, wc));
   }
}

void
ConnectionPool::conditionalRelease(weak_ptr<Connection> wc)
{
   shared_ptr<Connection> c = wc.lock();

   if (c && !c->active())
   {
      release(c);

      DebugLog(<< "dead connection pruned from pool: " << (void*)c.get());
   }
}

// Copyright (c) 2006, 2007 Chris L. Bond.
// 
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
