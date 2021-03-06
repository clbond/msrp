#ifndef MSRP_MESSAGE_HXX
#define MSRP_MESSAGE_HXX

#include <map>
#include <ostream>
#include <string>

#include <boost/shared_ptr.hpp>

#include <asio/buffer.hpp>

#include <rutil/Data.hxx>

#include "msrp/Header.hxx"
#include "msrp/ParseException.hxx"

namespace msrp
{

namespace parser
{
struct Message;
}

class Message
{
   public:
      enum Method
      {
         AUTH,
         SEND,
         REPORT,
         Response
      };

      enum MsgStatus
      {
         Continued,   // Message segment is complete (------ID+)
         Complete,    // Entire message is received and stored in Message class (-----ID$)
         Interrupted, // Message was cancelled (-----ID#)
         Streaming    // More contents are going to arrive for this message (no ID tag)
      };

      // !cb! You may choose to instantiate Message in any way you like--the
      // constructor is public--but if you use the factory method, you get the
      // advantage of allocating from an object pool specialized for Message
      // objects.  In a relay situation especially, this results in reduced
      // CPU consumption and decreased heap fragmentation.  The object is
      // automatically freed back to the pool on destruction.

      // parse a message from network buffer contents
      static boost::shared_ptr<Message> factory(const asio::const_buffer&);
      static boost::shared_ptr<Message> factory();

      Message();

      // Create a response template from this message.
      boost::shared_ptr<Message> response(unsigned int code, const std::string& phrase) const;

      const std::string& transaction() const { return mTransaction; }
      std::string& transaction() { return mTransaction; }

      const unsigned int statusCode() const { return mStatusCode; }
      unsigned int& statusCode() { return mStatusCode; }

      const std::string& statusPhrase() const { return mStatusPhrase; }
      std::string& statusPhrase() { return mStatusPhrase; }

      const Method& method() const { return mMethod; }
      Method& method() { return mMethod; }

      const MsgStatus status() const { return mStatus; }
      MsgStatus& status() { return mStatus; }

      const resip::Data& contents() const { return mContents; }
      resip::Data& contents() { return mContents; }

      // !cb! Note that the const and non-const header() methods have
      // different behaviour when referring to a non-existent header.
      // The const version will throw a ParseException and the other one
      // will create the header and return a reference to its value.
      // (It will still throw if it contains a value that it can't parse.)
      // If you want to overwrite the value of a header, always use the
      // headerRef method to avoid parsing at all.
      template<typename HeaderT>
      const typename HeaderT::Value& header() const
      {
         return lazyStorage<HeaderT>().getConst(mHeaders);
      }

      template<typename HeaderT>
      typename HeaderT::Value& header()
      {
         return lazyStorage<HeaderT>().get(mHeaders);
      }

      // !cb! If you wish to assign to a header and discard any value that
      // may have previously existed, use headerRef to avoid potentially
      // throwing an exception.
      template<typename HeaderT>
      typename HeaderT::Value& headerRef()
      {
         return lazyStorage<HeaderT>().value();
      }

      // extension headers
      const std::string& header(const std::string& key) const
      {
         using std::map;
         using std::string;

         map<string, string>::const_iterator i = mHeaders.find(key);
         if (i == mHeaders.end())
         {
            throw ParseException(key, codeContext());
         }

         return i->second;
      }

      std::string& header(const std::string& key)
      {
         return mHeaders[key];
      }

      template<typename HeaderT>
      bool exists() const
      {
         return lazyStorage<HeaderT>().parsed() || exists(HeaderT::Key);
      }

      // !cb! It is important that you not use this for known headers that
      // have their own typedefs, otherwise it may give false negatives.
      // Use the proper exists<T>() for those instead.
      bool exists(const std::string& key) const
      {
         return mHeaders.find(key) != mHeaders.end();
      }

      // Parse all header contents up front.  This may be desirable in
      // applications where performance is not critical and you don't
      // want to worry about surrounding all header<> calls in try-catch
      // blocks.  (Note: preparse() itself can throw a ParseException,
      // but it lets you at least reduce the number of try-catch blocks
      // required by catching malformed headers right off the bat.)
      void preparse();

      // Prepare message for transmission -- create random transaction and
      // message IDs if they are not already set.  Note that a Message is
      // not guaranteed to be ready for transmission after calling this
      // since it may still lack To-Path or From-Path headers.  The method
      // Session::prepare guarantees a wire-ready message but may not fill
      // in the correct To-Path since it only knows the remote IP address.
      bool prepare();

      std::ostream& encodeHeader(std::ostream&) const;
      std::ostream& encodeContents(std::ostream&) const;

   public:
      friend struct parser::Message;

      // unparsed
      mutable std::map<std::string, std::string> mHeaders;

      // parsed
      #define DefineHeader(h) h lazy##h
      DefineHeader(FromPath);           // From-Path
      DefineHeader(ToPath);             // To-Path
      DefineHeader(UsePath);            // Use-Path
      DefineHeader(MessageId);          // Message-ID
      DefineHeader(ContentLength);      // Content-Length
      DefineHeader(ContentType);        // Content-Type
      DefineHeader(ByteRange);          // Byte-Range
      DefineHeader(Expires);            // Expires
      DefineHeader(MinExpires);         // Min-Expires
      DefineHeader(Status);             // Status
      DefineHeader(SuccessReport);      // Success-Report
      DefineHeader(FailureReport);      // Failure-Report
#ifdef ENABLE_AUTHTUPLE
      DefineHeader(WWWAuthenticate);    // WWW-Authenticate
      DefineHeader(AuthenticationInfo); // Authentication-Info
      DefineHeader(Authorization);      // Authorization
#endif
      #undef DefineHeader

      template<typename HeaderT>
      const HeaderT& lazyStorage() const
      {
         return const_cast<Message*>(this)->lazyStorage<HeaderT>();
      }

      template<typename HeaderT>
      HeaderT& lazyStorage()
      {
         std::abort();
      }

      unsigned int mStatusCode;

      std::string mTransaction;
      std::string mStatusPhrase;

      resip::Data mContents;

      Method mMethod;

      MsgStatus mStatus;
};

std::ostream&
operator<<(std::ostream&, const Message&);

#define HeaderLinkage(h) \
   template<> inline h& \
   Message::lazyStorage<h>() \
   { \
      return lazy##h; \
   }
HeaderLinkage(FromPath);
HeaderLinkage(ToPath);
HeaderLinkage(UsePath);
HeaderLinkage(MessageId);
HeaderLinkage(ContentLength);
HeaderLinkage(ContentType);
HeaderLinkage(ByteRange);
HeaderLinkage(Expires);
HeaderLinkage(MinExpires);
HeaderLinkage(Status);
HeaderLinkage(SuccessReport);
HeaderLinkage(FailureReport);
#ifdef ENABLE_AUTHTUPLE
HeaderLinkage(WWWAuthenticate);
HeaderLinkage(AuthenticationInfo);
HeaderLinkage(Authorization);
#endif
#undef HeaderLinkage

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
