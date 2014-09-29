#ifndef MSRP_EXCEPTION_HXX
#define MSRP_EXCEPTION_HXX

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <boost/filesystem/path.hpp>

namespace msrp
{

typedef std::pair<const std::string, unsigned int> ExceptionContext;

#define codeContext() std::make_pair(__FILE__, __LINE__)

class Exception : public std::runtime_error
{
   public:
      Exception(const std::string& msg, const ExceptionContext& context)
         : std::runtime_error(format(msg, context))
      {}

   private:
      static const std::string format(const std::string& msg,
            const ExceptionContext& context)
      {
         std::stringstream ss;

         boost::filesystem::path p(context.first, boost::filesystem::no_check);

         ss << p.leaf()
            << ':' << context.second
            << ": "
            << msg;

         return ss.str();
      }
};

std::ostream&
operator<<(std::ostream&, const Exception&);

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
