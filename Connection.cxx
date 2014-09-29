#include <algorithm>
#include <cassert>
#include <functional>

#ifdef check
   #undef check
#endif

#include "msrp/System.hxx"
#include "msrp/Connection.hxx"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>

#include <rutil/Inserter.hxx>
#include <rutil/Logger.hxx>

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::TRANSPORT

using namespace msrp;
using namespace std;
using namespace boost;
using namespace resip;
using namespace asio;
using namespace asio::ip;


boost::shared_ptr<Connection> 
Connection::createAnswer(asio::io_service& service,
      const std::vector<asio::ip::tcp::endpoint>& targets,
      const boost::shared_ptr<asio::ssl::context> identity)
{
   boost::shared_ptr<Connection> c(new Connection(service, targets, identity));

   c->initOffer();

   return c;
}


boost::shared_ptr<Connection> 
Connection::createOffer(asio::io_service& service,
      const asio::ip::tcp::endpoint& bind,
      const boost::shared_ptr<asio::ssl::context> identity)
{
   boost::shared_ptr<Connection> c(new Connection(service, bind, identity));

   c->listen(bind);

   return c;
}

Connection::Connection(io_service& service,
      const vector<tcp::endpoint>& targets,
      const shared_ptr<ssl::context> identity) :
   mService (service),
   mIdentity(identity),
   mTargets (targets),
   mTarget  (targets.begin()),
   mState   (Disconnected),
   mDepends (0)
{}

Connection::Connection(io_service& service,
      const tcp::endpoint& bind,
      const shared_ptr<ssl::context> identity) :
   mService (service),
   mIdentity(identity),
   mTargets (),
   mTarget  (mTargets.end()),
   mDepends (0)
{
}

Connection::Connection(io_service& service, auto_ptr<tcp::socket> stream) :
   mService(service),
   mTargets(),
   mTarget (mTargets.end()),
   mTcp    (stream),
   mDepends(0)
{
   init();
}

Connection::Connection(io_service& service,
      auto_ptr<ssl::stream<tcp::socket> > stream) :
   mService(service),
   mTargets(),
   mTarget (mTargets.end()),
   mTls    (stream),
   mDepends(0)
{
   init();
}

void 
Connection::initOffer()
{
   if (!mTargets.empty())
   {
      mTarget = mTargets.begin();

      connect();
   }
   else
   {
      mTarget = mTargets.end();
   }
}

void
Connection::init()
{
   assert(active());

   tcp::endpoint remote;
   
   try
   {
      remote = socket().remote_endpoint();
   }
   catch (const asio::error_code&)
   {}

   if (remote == tcp::endpoint())
   {
      mState = Disconnected;
   }
   else
   {
      mState = Connected;

      mTargets.push_back(remote);
   }
}

Connection::~Connection()
{
}

Connection::State
Connection::state() const
{
   return mState;
}

io_service&
Connection::service() const
{
   return mService;
}

// incoming message demultiplexer
Demultiplex&
Connection::demultiplexer()
{
   return mDemux;
}

// outgoing message scheduler
Scheduler&
Connection::scheduler()
{
   return mScheduler;
}

// outgoing stream context
StreamContext&
Connection::context()
{
   return mContext;
}

unsigned int
Connection::dependents() const
{
   return mDepends;
}

unsigned int&
Connection::dependents()
{
   return mDepends;
}

const vector<ip::tcp::endpoint>&
Connection::targets() const
{
   return mTargets;
}

void
Connection::pushTargets(const vector<tcp::endpoint>& ve)
{
   bool reposition = mTarget == mTargets.end();

   mTargets.insert(mTargets.end(), ve.begin(), ve.end());

   unique(mTargets.begin(), mTargets.end());

   if (reposition)
   {
      mTarget = find(mTargets.begin(), mTargets.end(), ve.front());
   }

   if (mState == Disconnected && mReconnectTimer.get() == 0)
   {
      connect();
   }
}

unsigned int
Connection::remainingTargets() const
{
   return distance(mTarget, mTargets.end());
}

bool
Connection::active() const
{
   return mTcp || mTls || mReconnectTimer;
}

bool
Connection::tls() const
{
   return static_cast<bool>(mTls);
}

const tcp::endpoint
Connection::peer() const
{
   try
   {
      if (active())
      {
         return socket().remote_endpoint();
      }
   }
   catch (const asio::error_code&)
   {}

   return tcp::endpoint();
}

const tcp::endpoint
Connection::local() const
{
   try
   {
      if (active())
      {
         return socket().local_endpoint();
      }
   }
   catch (const asio::error_code&)
   {}

   return tcp::endpoint();
}

const ip::address
Connection::address() const
{
   return peer().address();
}

void
Connection::selectOutgoing()
{
   mContext.select(scheduler());
}

void
Connection::send(const const_buffer& buf)
{
   size_t bytes = 0;

   if (active())
   {
      if (mSend.empty())
      {
         if (mTls)
         {
            bytes = mTls->write_some(const_buffers_1(buf));
         }
         else if (mTcp)
         {
            bytes = mTcp->write_some(const_buffers_1(buf));
         }
      }
   }

   // !cb! If there is data in the send queue, it means an asynchronous write
   // has been started on this stream, so we can just append to the buffer and
   // it will get sent out once the previous write has completed.

   if (buffer_size(buf) > bytes)
   {
      resip::Data data(resip::Data::Borrow,
         buffer_cast<const char*>(buf) + bytes,
         buffer_size(buf) - bytes);

      bool idle = mSend.empty();

      mSend.write(data);

      if (idle)
      {
         write();
      }
   }
   else if (bytes > 0)
   {
      // !cb! post a write callback to invoke the message scheduler
      service().post(bind(&Connection::writeHandler, shared_from_this(), false, asio::error_code(), bytes));
   }
}

// !cb! write from send buffer
void
Connection::write()
{
   if (mTls)
   {
      async_write(*mTls,
         mSend.const_buffers(),
         bind(&Connection::writeHandler,
            shared_from_this(),
            true, // buffered
            placeholders::error,
            placeholders::bytes_transferred));
   }
   else if (mTcp)
   {
      async_write(*mTcp,
         mSend.const_buffers(),
         bind(&Connection::writeHandler,
            shared_from_this(),
            true, // buffered
            placeholders::error,
            placeholders::bytes_transferred));
   }
}

void
Connection::writeHandler(bool queued, const asio::error_code& e, size_t bytes)
{
   if (e)
   {
      if (mState != Disconnected && e != error::operation_aborted)
      {
         disconnect(e);
      }

      return;
   }

   if (queued)
   {
      mSend.shift(bytes);
   }

   if (!mSend.empty())
   {
      write();
   }
   else
   {
      selectOutgoing();
   }
}

void
Connection::receive(const mutable_buffer& mb)
{
   assert(active());

   mutable_buffers_1 buffer(mb);

   if (mTls)
   {
      mTls->async_read_some(buffer,
         bind(&Connection::receiveHandler,
            shared_from_this(),
            placeholders::error,
            placeholders::bytes_transferred));
   }
   else if (mTcp)
   {
      mTcp->async_receive(buffer,
         bind(&Connection::receiveHandler,
            shared_from_this(),
            placeholders::error,
            placeholders::bytes_transferred));
   }
}

void
Connection::receiveHandler(const asio::error_code& e, size_t bytes)
{
   if (mState == Disconnected)
   {
      return;
   }

   if (e)
   {
      if (mState != Disconnected && e != error::operation_aborted)
      {
         disconnect(e);
      }
   }
   else
   {
      mBuffer.read(bytes);

      switch (mBuffer.state())
      {
         case MessageBuffer::Status:
         case MessageBuffer::Headers:
            break;
         case MessageBuffer::Content:
            if (mBuffer.method() == Message::SEND)
            {
               // !cb! SEND requests are the only message types that we want to
               // process before they have been completely received, because they
               // are the only messages that may potentially exceed the message
               // buffer size - and they may take quite a while to complete, which
               // is not true of AUTH and REPORT requests, or responses.
               process();
            }
            break;
         case MessageBuffer::Complete:
            process();
            break;
         default:
            break;
      }

      // !cb! handler may have closed the connection
      if (active())
      {
         receive(mBuffer.mutableBuffer());
      }
   }
}

void
Connection::process()
{
   try
   {
      if (mDemux.streaming())
      {
         mDemux.process(mBuffer.contents(), mBuffer.status());
      }
      else
      {
         if (mBuffer.state() == MessageBuffer::Complete)
         {
            shared_ptr<Message> m = mBuffer.parse(MessageBuffer::CopyContents);
            if (m)
            {
               if (!mDemux.process(m))
               {
                  reject(m, 481);
               }
            }
         }
         else
         {
            shared_ptr<Message> m = mBuffer.parse(MessageBuffer::NoContents);
            if (m)
            {
               if (mDemux.process(m))
               {
                  const const_buffer buffer = mBuffer.contents();

                  if (buffer_size(buffer) > 0)
                  {
                     mDemux.process(mBuffer.contents(), mBuffer.status());
                  }
               }
               else
               {
                  reject(m, 481);
               }
            }
         }
      }

      switch (mBuffer.state())
      {
         case MessageBuffer::Content:
            mBuffer.erase();
            break;
         case MessageBuffer::Complete:
            mBuffer.reset();
            break;
         default:
            std::abort();
      }
   }
   catch (const ParseException& e)
   {
      WarningLog(<< "parse exception while processing: " << e.what());
   }
   catch (const msrp::Exception& e)
   {
      WarningLog(<< "unknown exception while processing: " << e.what());
   }

   // !cb! If we can't parse the request, we ought to send a 400 response
   // to indicate this to the sender - but how can we route the message if
   // we can't do enough parsing to fetch the To and From path?
}

void
Connection::reject(shared_ptr<const Message> m, unsigned int code)
{
   DebugLog(<< "rejecting message with code " << code);

   shared_ptr<Message> response = m->response(code, "Rejected");
   if (response)
   {
      send(response);
   }
}

void
Connection::send(shared_ptr<const Message> m)
{
   // ensure that the stream is not in the middle of sending another chunk
   mContext.clear();

   stringstream ss;
   ss << *m;

   const string& s = ss.str();

   send(const_buffer(s.c_str(), s.size()));
}

const tcp::endpoint
Connection::getTarget()
{
   if (mTargets.empty())
   {
      throw Exception("no endpoints in target list", codeContext());
   }

   // cycle
   if (mTarget == mTargets.end())
   {
      mTarget = mTargets.begin();
   }

   return *mTarget;
}

Connection::TcpStream::lowest_layer_type&
Connection::socket() const
{
   if (mTls)
   {
      return mTls->lowest_layer();
   }
   else if (mTcp)
   {
      return mTcp->lowest_layer();
   }

   throw Exception("no stream open", codeContext());
}

void
Connection::createStream(bool ip6, bool open)
{
   if (mIdentity)
   {
      mTls.reset(new ssl::stream<tcp::socket>(service(), *mIdentity));
   }
   else
   {
      mTcp.reset(new asio::ip::tcp::socket(service()));
   }

   if (open)
   {
      TcpStream::lowest_layer_type& s = socket();
   
      if (ip6)
      {
         s.open(tcp::v6());
      }
      else
      {
         s.open(tcp::v4());
      }
   
      // socket options
      s.set_option(tcp::no_delay(true));
      s.set_option(socket_base::reuse_address(true));
      s.set_option(socket_base::keep_alive(true));
   }
}

void
Connection::connect()
{
   assert(mState == Disconnected);

   try
   {
      const tcp::endpoint target = getTarget();

      createStream(target.address().is_v6(), true);

      mState = Connecting;

      // connect
      socket().async_connect(target,
         bind(&Connection::connectHandler, shared_from_this(), placeholders::error));

      mConnecting(target);

      InfoLog(<< "Connecting: " << local() << "->" << target);

      return;
   }
   catch (const asio::error_code& e)
   {
      ErrLog(<< "Connect error");
      throw e;
   }
   catch (const Connection::Exception& e)
   {
      ErrLog(<< "Connect error: " << e.what());
      throw e;
   }
}

void
Connection::reconnect(const deadline_timer::duration_type& duration)
{
   if (duration != deadline_timer::duration_type(0, 0, 0))
   {
      if (mReconnectTimer)
      {
         try
         {
            mReconnectTimer->cancel();
         }
         catch (const asio::error_code&) {}
      }
      else
      {
         mReconnectTimer.reset(new deadline_timer(service()));
      }
   
      mReconnectTimer->expires_from_now(duration);
      mReconnectTimer->async_wait(
         bind(&Connection::reconnectHandler, shared_from_this(), placeholders::error));

      InfoLog(<< "Reconnecting at "
              << posix_time::to_simple_string(mReconnectTimer->expires_at()));
   }
   else
   {
      InfoLog(<< "Reconnecting");

      connect();
   }
}

void
Connection::connectHandler(const asio::error_code& e)
{
   if (e)
   {
      if (mState != Disconnected && e != error::operation_aborted)
      {
         disconnect(e);
      }
   }
   else
   {
      InfoLog(<< "Connected: " << local() << "->" << peer());

      mState = Connected;

      mConnect(peer());

      if (!mSend.empty())
      {
         write();
      }

      receive(mBuffer.mutableBuffer());
   }
}

void
Connection::reconnectHandler(const asio::error_code& e)
{
   if (!e)
   {
      connect();
   }
   else
   {
      // timer cancelled
   }

   mReconnectTimer.reset();
}

void
Connection::disconnect(const asio::error_code& e)
{
   if (mState == Disconnected)
   {
      return;
   }

   InfoLog(<< (void*)this << " Disconnected: " << e);

   mState = Disconnected;

   mTls.reset();
   mTcp.reset();

   if (e)
   {
      ++mTarget;

      if (mTarget != mTargets.end())
      {
         reconnect(deadline_timer::duration_type(0, 0, 0));
      }
      else if (!mDisconnect.empty())
      {
         mDisconnect(e);
      }
   }
   else if (!mDisconnect.empty())
   {
      mDisconnect(e);
   }
}

void
Connection::listen(const tcp::endpoint& endpoint)
{
   mAccept.reset(new tcp::acceptor(service()));

   if (endpoint.address().is_v6())
   {
      mAccept->open(tcp::v6());
   }
   else
   {
      mAccept->open(tcp::v4());
   }

   // set socket options
   mAccept->set_option(tcp::no_delay(true));
   mAccept->set_option(socket_base::reuse_address(true));

   mAccept->bind(endpoint);

   mAccept->listen();

   // stream to accept the connection with
   createStream(endpoint.address().is_v6(), false);

   mAccept->async_accept(socket(),
      bind(&Connection::acceptHandler,
         shared_from_this(),
         placeholders::error));

   mState = Listening;

   mListen(mAccept->local_endpoint());

   InfoLog(<< "Listening on " << endpoint);
}

void
Connection::acceptHandler(const asio::error_code& e)
{
   if (e)
   {
      disconnect(e);
   }
   else
   {
      InfoLog(<< "Accepted connection from " << peer());

      mAccept.reset();

      mState = Connected;

      mConnect(peer());
   }
}

void
Connection::close()
{
   disconnect(asio::error_code());
}

signal1<void, const asio::ip::tcp::endpoint>&
Connection::onListen()
{
   return mListen;
}

signal1<void, const asio::ip::tcp::endpoint>&
Connection::onConnecting()
{
   return mConnecting;
}

signal1<void, const asio::ip::tcp::endpoint>&
Connection::onConnect()
{
   return mConnect;
}

signal1<void, const asio::error_code&>&
Connection::onDisconnect()
{
   return mDisconnect;
}

ostream&
msrp::operator<<(ostream& os, const Connection& c)
{
   switch (c.state())
   {
      case Connection::Listening:
         os << "Listening " << c.local();
         break;

      case Connection::Connecting:
         os << "Connecting " << c.local() << "->" << c.peer();
         break;

      case Connection::Handshaking:
         os << "Handshake " << c.local() << "->" << c.peer();
         break;

      case Connection::Connected:
         os << "Connected " << c.local() << "->" << c.peer();
         break;

      case Connection::Disconnected:
         os << "Disconnected";

         if (!c.targets().empty())
         {
            os << ' ' << resip::Inserter(c.targets());
         }

         if (c.mReconnectTimer)
         {
            os << "[reconnect "
               << posix_time::to_simple_string(c.mReconnectTimer->expires_at())
               << ']';
         }

         break;
   }

   return os;
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
