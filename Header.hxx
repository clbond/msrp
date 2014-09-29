#ifndef MSRP_HEADER_HXX
#define MSRP_HEADER_HXX

#include <string>

#include <boost/spirit.hpp>

#include "msrp/AuthParser.hxx"
#include "msrp/AuthTuple.hxx"
#include "msrp/ByteRange.hxx"
#include "msrp/LazyField.hxx"
#include "msrp/MessageId.hxx"
#include "msrp/MimeParser.hxx"
#include "msrp/ParseUri.hxx"
#include "msrp/Report.hxx"
#include "msrp/Status.hxx"

namespace msrp
{

struct ContentLength :
   public LazyField::Storage<
      ContentLength,
      unsigned int,
      boost::spirit::uint_parser<unsigned, 10>
   >
{
   static const std::string Key;
};

struct ContentType :
   public LazyField::Storage<ContentType, Mime, parser::Mime>
{
   static const std::string Key;
};

struct ByteRange :
   public LazyField::Storage<ByteRange, ByteRangeTuple, parser::ByteRange>
{
   static const std::string Key;

   static const size_t Unknown;
};

struct SuccessReport :
   public LazyField::Storage<SuccessReport, bool, parser::SuccessReport>
{
   static const std::string Key;
};

struct FailureReport :
   public LazyField::Storage<FailureReport, int, parser::FailureReport>
{
   static const std::string Key;

   enum Report
   {
      No      = 0,
      Yes     = 1,
      Partial = 2
   };
};

struct FromPath : public LazyField::Storage<FromPath, Path, parser::Path>
{
   static const std::string Key;
};

struct ToPath : public LazyField::Storage<ToPath, Path, parser::Path>
{
   static const std::string Key;
};

struct UsePath : public LazyField::Storage<UsePath, Path, parser::Path>
{
   static const std::string Key;
};

struct MessageId :
   public LazyField::Storage<MessageId, std::string, parser::MessageId>
{
   static const std::string Key;
};

struct Status : public LazyField::Storage<Status, StatusTuple, parser::Status>
{
   static const std::string Key;
};

struct Expires :
   public LazyField::Storage<
      Expires,
      unsigned int,
      boost::spirit::uint_parser<unsigned>
   >
{
   static const std::string Key;
};

struct MinExpires :
   public LazyField::Storage<
      MinExpires,
      unsigned int,
      boost::spirit::uint_parser<unsigned>
   >
{
   static const std::string Key;
};

struct MaxExpires :
   public LazyField::Storage<
      MaxExpires,
      unsigned int,
      boost::spirit::uint_parser<unsigned>
   >
{
   static const std::string Key;
};

#ifdef ENABLE_AUTHTUPLE

struct WWWAuthenticate :
   public LazyField::Storage<WWWAuthenticate, AuthTuple, parser::Auth<true> >
{
   static const std::string Key;
};

struct Authorization :
   public LazyField::Storage<Authorization, AuthTuple, parser::Auth<true> >
{
   static const std::string Key;
};

struct AuthenticationInfo :
   public LazyField::Storage<AuthenticationInfo, AuthTuple, parser::Auth<false> >
{
   static const std::string Key;
};

#endif // ENABLE_AUTHTUPLE

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
