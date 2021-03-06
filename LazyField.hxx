#ifndef MSRP_LAZYFIELD_HXX
#define MSRP_LAZYFIELD_HXX

#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include <boost/spirit.hpp>
#include <boost/spirit/actor.hpp>

#include "msrp/Exception.hxx"
#include "msrp/ParseException.hxx"
#include "msrp/ParserFactory.hxx"

namespace msrp
{

struct LazyField
{
   template<
      typename FieldT,
      typename T,
      typename GrammarT>
   struct Storage
   {
      typedef T Value;
      typedef GrammarT Grammar;

      Storage() :
         mParsed(false)
      {}

      inline void parse(const std::string& s)
      {
         const Grammar& grammar = ParserFactory<Grammar>::get();

         boost::spirit::parse_info<> pi = boost::spirit::parse(
            &s.c_str()[0],
            &s.c_str()[s.size()],
            grammar[boost::spirit::assign_a(mValue)]);

         if (!pi.full)
         {
            throw ParseException(s, codeContext());
         }

         mParsed = true;
      }

      typedef std::map<std::string, std::string> Map;

      inline T& get(Map& map)
      {
         if (!parsed())
         {
            Map::iterator i = map.find(FieldT::Key);
            if (i != map.end())
            {
               parse(i->second);

               map.erase(i);
            }
         }

         return value();
      }

      inline const T& getConst(const Map& map) const
      {
         if (!parsed())
         {
            Map& nc = const_cast<Map&>(map);

            Map::iterator i = nc.find(FieldT::Key);
            if (i == nc.end())
            {
               throw ParseException(FieldT::Key, codeContext());
            }

            const_cast<Storage&>(*this).parse(i->second);

            nc.erase(i);
         }

         return mValue;
      }

      inline T& value()
      {
         mParsed = true;

         return mValue;
      }

      inline bool parsed() const
      {
         return mParsed;
      }

   private:
      bool mParsed;

      T mValue;
   };

   struct Data
   {
      typedef std::string Value;
   };
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
