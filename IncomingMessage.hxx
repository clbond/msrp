#ifndef MSRP_INCOMINGMESSAGE_HXX
#define MSRP_INCOMINGMESSAGE_HXX

#include <asio/buffer.hpp>
#include <asio/deadline_timer.hpp>

#include <boost/weak_ptr.hpp>

#include "msrp/Header.hxx"
#include "msrp/MessageSessionBase.hxx"

namespace msrp
{

class Demultiplex;
class Message;
class Session;

class IncomingMessage :
   public boost::noncopyable,
   public MessageSessionBase
{
   public:
      class SuccessReporting
      {
         public:
            enum Mode
            {
               // Let the stack decide how best to send success reports if they
               // have been requested by the remote party.
               Automatic,

               // Send incremental success reports covering the entire byte range of
               // contents received thus far.  Reports will be sent after a specific
               // number of bytes has been received, configured with incremental().
               Incremental,

               // Send individual success reports for each message fragment.
               Fragmented,

               // Send one success report after the entire message has been received.
               Completed,

               // Do not send success reports.
               None
            };

            SuccessReporting(const Mode m = Automatic) :
               mMode(m), mIncremental(0)
            {}

            const Mode mode() const
            {
               return mMode;
            }
            Mode& mode()
            {
               return mMode;
            }

            // Number of bytes that will trigger an Incremental report.
            const std::size_t incremental() const
            {
               return mIncremental;
            }
            std::size_t& incremental()
            {
               return mIncremental;
            }

         private:
            Mode mMode;

            std::size_t mIncremental;
      };

      IncomingMessage(boost::shared_ptr<Session>, const Message&);

      virtual ~IncomingMessage();

      const Message& getMessage() const;

      // send a 413 rejection response
      virtual void cancel();

      // configure success reporting
      const SuccessReporting& reports() const;
      SuccessReporting& reports();

      // failure reporting
      const FailureReport::Report& failureReports() const;

      boost::signal1<void, const Message&>& onContext();

      boost::signal1<void, asio::const_buffer>& onContents();

      boost::signal1<void, Message&>& onSendReport();

      boost::signal0<void>& onInterrupt();

   private:
      friend class Demultiplex;
      friend class Session;

      bool process(boost::shared_ptr<const Message>);

      bool process(const asio::const_buffer&);

      void continued();
      void completed();
      void interrupt();

      void successReport();

      boost::weak_ptr<Session> mSession;

      Message mMsg;

      SuccessReporting mReports;

      FailureReport::Report mFailureReports;

      std::size_t mFragmentStart;
      std::size_t mFragmentSize;

      boost::signal1<void, const Message&> mContext;
      boost::signal1<void, asio::const_buffer> mContentsEvent;
      boost::signal1<void, Message&> mSendReport;
      boost::signal0<void> mInterrupt;
};

}

#endif

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
