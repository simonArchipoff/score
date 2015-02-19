#pragma once
#include <QTime>
#include <boost/optional.hpp>
#include <chrono>

//using namespace std::literals::chrono_literals;

class PositiveInfinity{};
template<typename T>
class TimeValue_T
{
	public:
		TimeValue_T() = default;
		TimeValue_T(PositiveInfinity):
			m_impl{}
		{ }

		// These two overloads are here to please coverity...
		TimeValue_T(std::chrono::seconds&& dur):
			m_impl{std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()}
		{ }
		TimeValue_T(std::chrono::milliseconds&& dur):
			m_impl{dur.count()}
		{ }

		template<typename Duration,
				 typename std::enable_if<std::is_class<Duration>::value>::type* = nullptr >
		TimeValue_T(Duration&& dur):
			m_impl{std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()}
		{ }



		bool isInfinite() const
		{ return !bool(m_impl); }
		
		bool isZero() const
		{ return !isInfinite() && (msec() == 0); }

		int msec() const
		{ return *m_impl; }

		void addMSecs(int msecs)
		{ if(m_impl) *m_impl += msecs; }

		void setMSecs(int msecs)
		{ m_impl = msecs; }

		bool operator==(const TimeValue_T& other) const
		{
			return other.m_impl == m_impl;
		}

		bool operator!=(const TimeValue_T& other) const
		{
			return other.m_impl != m_impl;
		}

		bool operator>(const TimeValue_T& other) const
		{
			if(isInfinite() && other.isInfinite())
			{
				return false;
			}
			else if(isInfinite() && !other.isInfinite())
			{
				return true;
			}
			else if(!isInfinite() && other.isInfinite())
			{
				return false;
			}
			else
			{
				return msec() > other.msec();
			}
		}

		TimeValue_T operator+(const TimeValue_T& other)
		{
			TimeValue_T res{PositiveInfinity{}};
			if(isInfinite() || other.isInfinite())
			{
				return res;
			}
			res.m_impl = m_impl.get() + other.m_impl.get();
			return res;
		}

		TimeValue_T operator*(double other)
		{
			TimeValue_T res{PositiveInfinity{}};
			if(isInfinite())
			{
				return res;
			}
			res.m_impl = m_impl.get() * other;
			return res;
		}

		TimeValue_T operator-(const TimeValue_T& other)
		{
			TimeValue_T res{PositiveInfinity{}};
			if(isInfinite() || other.isInfinite())
			{
				return res;
			}
			res.m_impl = m_impl.get() - other.m_impl.get();
			return res;
		}



	private:
		boost::optional<T> m_impl{0};
};


/*
template<>
class TimeValue_T<QTime>
{
	public:
		TimeValue_T<QTime>() = default;
		TimeValue_T<QTime>(int msec):
			TimeValue_T{}
		{ m_impl.addMSecs(msec); }

		int msec() const
		{ return m_impl.msec(); }

		void addMSecs(int msecs)
		{ m_impl.addMSecs(msecs); }

		bool operator==(const TimeValue_T& other) const
		{ return other.m_impl == m_impl; }

		bool operator!=(const TimeValue_T& other) const
		{ return other.m_impl != m_impl; }

	private:
		QTime m_impl{0, 0};
};
*/

using  = TimeValue_T<int>;
inline QDebug operator<< (QDebug d, const TimeValue &tv) 
{
	if(!tv.isInfinite()) 
		d << tv.msec() << "ms";
	else
		d << "infinite";
	return d;
}
