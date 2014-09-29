#include <cassert>

#include <rutil/Logger.hxx>

#ifdef check
   #undef check
#endif

#include "msrp/System.hxx"
#include "msrp/IncomingMessage.hxx"
#include "msrp/Message.hxx"
#include "msrp/Session.hxx"

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::NONE

using namespace msrp;
using namespace std;
using namespace boost;
using namespace asio;

IncomingMessage::IncomingMessage(shared_ptr<Session> s, const Message& m) :
   MessageSessionBase(m), mSession(s), mMsg(m), mFragmentSize(0)
{
   mFailureReports = FailureReport::Yes;

   try
   {
      if (m.exists<SuccessReport>() &&
          m.header<SuccessReport>() == false)
      {
         mReports.mode() = SuccessReporting::None;
      }

      if (m.exists<FailureReport>())
      {
         mFailureReports = static_cast<FailureReport::Report>(m.header<FailureReport>());
      }
   }
   catch (const ParseException&)
   {}
}

IncomingMessage::~IncomingMessage()
{}

const Message&
IncomingMessage::getMessage() const
{
   return mMsg;
}

void
IncomingMessage::cancel()
{
   try
   {
      shared_ptr<Session> session(mSession);

      shared_ptr<const Message> response =
         message().response(413, "Cancelled");

      session->stream(*response);
   }
   catch (const ParseException&)
   {
      ErrLog(<< "cannot create response for invalid request");
   }
   catch (const bad_weak_ptr&)
   {
      ErrLog(<< "session is defunct, cannot cancel message");
   }
}

const IncomingMessage::SuccessReporting&
IncomingMessage::reports() const
{
   return mReports;
}

IncomingMessage::SuccessReporting&
IncomingMessage::reports()
{
   return mReports;
}

signal1<void, const Message&>&
IncomingMessage::onContext()
{
   return mContext;
}

signal1<void, asio::const_buffer>&
IncomingMessage::onContents()
{
   return mContentsEvent;
}

signal1<void, Message&>&
IncomingMessage::onSendReport()
{
   return mSendReport;
}

signal0<void>&
IncomingMessage::onInterrupt()
{
   return mInterrupt;
}

bool
IncomingMessage::process(shared_ptr<const Message> m)
{
   message().operator=(*m);

   mLastTransfer = posix_time::microsec_clock::local_time();

   mFragmentSize = 0;

   try
   {
      // !cb! Perhaps they have requested different success reporting.  I'm not
      // sure if this is correct behaviour - maybe we ought to query the
      // application for what it wants to do with reporting before just changing?
      if (message().exists<SuccessReport>())
      {
          if (message().header<SuccessReport>() == true)
          {
             if (mReports.mode() == SuccessReporting::None)
             {
                mReports.mode() = SuccessReporting::Automatic;
             }
          }
          else
          {
             mReports.mode() = SuccessReporting::None;
          }
      }

      // Failure reports?
      if (message().exists<FailureReport>())
      {
         unsigned int reports = message().header<FailureReport>();

         mFailureReports = static_cast<FailureReport::Report>(reports);
      }
   }
   catch (const ParseException&)
   {}

   if (!mContext.empty())
   {
      mContext(*m);

      return true;
   }

   return false;
}

bool
IncomingMessage::process(const const_buffer& b)
{
   const size_t size = buffer_size(b);

   if (mFragmentSize == 0)
   {
      mFragmentStart = mTransferred;
      mFragmentSize = size;
   }
   else
   {
      mFragmentSize += size;
   }

   mTransferred += size;

   mLastTransfer = posix_time::microsec_clock::local_time();

   if (!mContentsEvent.empty())
   {
      mContentsEvent(b);

      return true;
   }

   return false;
}

void
IncomingMessage::continued()
{
   if (mReports.mode() == SuccessReporting::Automatic ||
       mReports.mode() == SuccessReporting::Fragmented)
   {
      successReport();
   }
}

void
IncomingMessage::completed()
{
   mComplete = true;

   if (!onComplete().empty())
   {
      onComplete()();
   }

   if (mReports.mode() != SuccessReporting::None)
   {
      successReport();
   }
}

void
IncomingMessage::interrupt()
{
   mInterrupted = true;

   if (!onInterrupt().empty())
   {
      onInterrupt()();
   }
   else if (!onComplete().empty())
   {
      onComplete()();
   }
}

void
IncomingMessage::successReport()
{
   try
   {
      shared_ptr<Session> session(mSession);

      shared_ptr<Message> m = Message::factory();
      assert(m);

      if (!session->prepare(*m))
      {
         throw msrp::Exception("Cannot prepare message for transport", codeContext());
      }

      ByteRangeTuple& br = m->headerRef<ByteRange>();
      br.start = mFragmentStart;
      br.end = mFragmentStart + mFragmentSize;
      br.total = size();

      m->method() = msrp::Message::REPORT;

      session->stream(*m);
   }
   catch (const bad_weak_ptr&)
   {
      throw msrp::Exception("Cannot send success report, session is invalid", codeContext());
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
