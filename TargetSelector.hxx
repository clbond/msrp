#ifndef MSRP_TARGETSELECTOR_HXX
#define MSRP_TARGETSELECTOR_HXX

#include <list>
#include <map>
#include <string>

#include <rutil/Random.hxx>

#include <rutil/dns/DnsStub.hxx>
#include <rutil/dns/DnsSrvRecord.hxx>

#include "msrp/Exception.hxx"

namespace msrp
{

// !cb! RFC 2782 - select a service record target from a DNS result
class TargetSelector
{
   public:
      struct Exception : public msrp::Exception
      {
         Exception(const std::string& s, const ExceptionContext& context) :
            msrp::Exception(s, context)
         {}
      };

      TargetSelector(std::vector<resip::DnsSrvRecord> records)
      {
         std::sort(records.begin(), records.end(), WeightCompare());

         for (std::vector<resip::DnsSrvRecord>::const_iterator i = records.begin();
               i != records.end(); ++i)
         {
            mRecords[i->priority()].push_back(*i);
         }
      }

      const resip::DnsSrvRecord next()
      {
         if (mRecords.empty())
         {
            if (mRemoved.empty())
            {
               throw Exception("DNS SRV records exhausted", codeContext());
            }
            else
            {
               mRecords.swap(mRemoved);
            }
         }

         WeightMap map;
         sum(mRecords.begin()->first, map);

         WeightMap::const_iterator i =
            map.lower_bound(resip::Random::getRandom() % (highest(map) + 1));

		 assert(i != map.end());

         const resip::DnsSrvRecord record = i->second;

         recycle(mRecords.begin()->first, record);

         return record;
      }

   private:
      friend std::ostream& operator<<(std::ostream&, const TargetSelector&);

      typedef std::map<int, std::list<resip::DnsSrvRecord> > RecordMap;

      RecordMap mRecords;
      RecordMap mRemoved;

      struct WeightCompare :
         public std::binary_function<const resip::DnsSrvRecord, const resip::DnsSrvRecord, bool>
      {
         bool operator()(const resip::DnsSrvRecord& lhs,
                         const resip::DnsSrvRecord& rhs) const
         {
            return lhs.weight() < rhs.weight();
         }
      };

      struct CompareRecord :
         public std::unary_function<const resip::DnsSrvRecord, bool>
      {
         CompareRecord(const resip::DnsSrvRecord& r) :
            lhs(r)
         {}

         bool operator()(const resip::DnsSrvRecord& rhs) const
         {
            return lhs.priority() == rhs.priority()
               && lhs.weight() == rhs.weight()
               && lhs.port() == rhs.port()
               && lhs.target() == rhs.target()
               && lhs.name() == rhs.name();
         }

         const resip::DnsSrvRecord& lhs;
      };

      typedef std::map<int, const resip::DnsSrvRecord> WeightMap;

      void sum(int priority, WeightMap& map) const
      {
         RecordMap::const_iterator ri = mRecords.find(priority);
         assert(ri != mRecords.end());

         const std::list<resip::DnsSrvRecord>& records = ri->second;

         std::list<resip::DnsSrvRecord>::const_iterator i = records.begin();

         for (int running = 0; i != records.end(); ++i)
         {
            running += i->weight();

            map.insert(WeightMap::value_type(running, *i));
         }
      }

      int highest(const WeightMap& map) const
      {
         WeightMap::const_iterator i = map.end();
         --i;

         return i->first;
      }

      void recycle(int p, const resip::DnsSrvRecord& r)
      {
         std::list<resip::DnsSrvRecord>& recordList = mRecords[p];

         std::list<resip::DnsSrvRecord>::iterator i =
            std::find_if(recordList.begin(), recordList.end(), CompareRecord(r));

         if (i != recordList.end())
         {
            recordList.erase(i);

            if (recordList.empty())
            {
               mRecords.erase(p);
            }
         }

         mRemoved[p].push_back(r);
      }
};

std::ostream&
operator<<(std::ostream&, const TargetSelector&);

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
