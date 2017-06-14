#ifndef BUFFER_H
#define BUFFER_H

#ifndef ASSERT
#define ASSERT(statement) (void)((statement));
#endif

#include <cmath>
#include <deque>
#include <vector>
#include <memory>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef min
#undef min
#else
#endif

#ifdef max
#undef max
#else
#endif

#ifndef clamp
#define clamp(v, l, h) (((v)<(l))?(l):((v)>(h))?(h):(v))
#endif

namespace FilterLib {

typedef std::size_t size_t;
typedef float fsize_t;
typedef float time_t;

template<typename T>
using TimeValuePair = std::pair<time_t, T>;

enum SampleType
{
	Nearest = 0,
	Linear,
	Spline,
};


template<typename ValueT>
struct Buffer_T {
	typedef ValueT ValueType;
	static const ValueType zero;
//	static constexpr ValueType zero = Buffer_T<ValueT>::ValueType();
	static constexpr bool linear = false;
	static ValueType mix(ValueType a, ValueType b, fsize_t u) {
		return (u < fsize_t(0.5)) ? a : b;
	}
};

template<typename ValueT>
const typename Buffer_T<ValueT>::ValueType Buffer_T<ValueT>::zero =
	Buffer_T<ValueT>::ValueType();

template<>
struct Buffer_T<int> {
	typedef int ValueType;
	static const ValueType zero;
//	static constexpr ValueType zero = 0;
	static constexpr bool linear = true;
	static ValueType mix(ValueType a, ValueType b, fsize_t u) {
		return ValueType(std::round(a * (fsize_t(1.0) - u) + b * u));
	}
};

const Buffer_T<int>::ValueType Buffer_T<int>::zero =
	0;

template<>
struct Buffer_T<float> {
	typedef float ValueType;
	static const ValueType zero;
//	static constexpr ValueType zero = 0;
	static constexpr bool linear = true;
	static ValueType mix(ValueType a, ValueType b, fsize_t u) {
		return a * (fsize_t(1.0) - u) + b * u;
	}
};

const Buffer_T<float>::ValueType Buffer_T<float>::zero =
	0;


static_assert(Buffer_T<fsize_t>::linear,
	"fsize_t not interpolatable");
static_assert(Buffer_T<time_t>::linear,
	"time_t not interpolatable");


template<typename T>
class ProcessChain {
public:
	ProcessChain(ProcessChain* parent = nullptr) :
		m_index(0),
		m_parent(nullptr),
		m_child(nullptr),
		m_simbling(nullptr)
	{
		setParent(parent);
	}

	virtual ~ProcessChain() { }

	inline ProcessChain& operator<<(const T& input) {
		in(input);
		return *this;
	}
	inline ProcessChain& operator<<(const ProcessChain& prev) {
		in(prev.out());
		return *this;
	}
	inline T operator>>(T& output) const { return output = out(); }
	inline T operator>>(ProcessChain& next) const { return next.in(out()); }
	inline T operator()() const { return out(); }
	inline T operator()(const T& input) { return in(input); }
	inline T operator()(const ProcessChain& prev) { return in(prev.out()); }

	inline ProcessChain* parent() const { return m_parent; }
	inline void setParent(ProcessChain* parent) {
		ASSERT(m_parent == nullptr);
		if (parent != nullptr) {
			m_parent = parent;
			m_simbling = parent->m_child;
			m_index = (m_simbling == nullptr) ?
				0 :
				m_simbling->m_index + 1;
			parent->m_child = this;
		}
	}

	inline ProcessChain* first() const { return m_child; }
	inline ProcessChain* next() const { return m_simbling; }
	inline size_t index() const { return m_index; }

	virtual inline T in(const T& input) {
		T output = process(input);
		if (m_simbling != nullptr)
			m_simbling->in(input);
		if (m_child != nullptr)
			m_child->in(output);
		return output;
	}

	virtual T out() const = 0;

protected:
	size_t m_index;
	ProcessChain *m_parent, *m_child, *m_simbling;

	virtual T process(const T& input) = 0;
};

template<typename T>
std::ostream& operator<<(std::ostream& a, const ProcessChain<T>& b) {
	return a << b.out();
}

template<typename T>
inline T operator>>(const T& a, ProcessChain<T>& b) {
	return b.in(a);
}

template<typename T>
class AbstractBuffer {
public:
	typedef Buffer_T<T> trait;

	virtual ~AbstractBuffer() { }

	//inline std::vector<T>& operator>>(std::vector<T>& vector) const {
	//	to(vector);
	//	return vector;
	//}

	virtual T out() const = 0; // get current
	virtual T sample(fsize_t index, SampleType type = Linear) const = 0;
	virtual void to(std::vector<T>& vector) const = 0;

	virtual T in(const T& input) = 0;
	virtual void fill(const T& value) = 0;
};

template<typename T>
std::ostream& operator<<(std::ostream& a, const AbstractBuffer<T>& b) {
	std::stringstream ss;
	ss << "AbstractBuffer(" << b.out() << ")";
	a << ss.str();
	return a;
}

template<typename T>
class Buffer :
	public ProcessChain<T>,
	public AbstractBuffer<T>,
	public std::deque<T>
{
public:
	Buffer(size_t size, ProcessChain<T>* parent = nullptr) :
		ProcessChain<T>(parent),
		AbstractBuffer<T>(),
		std::deque<T>(size, Buffer<T>::trait::zero)
	{
		ASSERT(size > 1);
	}

	virtual ~Buffer() { }

	virtual inline std::string name() const { return m_name.empty() ? "Buffer" : m_name; }
	inline void setName(const std::string name) { m_name = name; }

	inline T out() const override {
		return std::deque<T>::front();
	}

	inline T sample(fsize_t index, SampleType type = Linear) const override {
		index = std::max(index, static_cast<fsize_t>(0));
		index = std::min(index, static_cast<fsize_t>(std::deque<T>::size() - 1));
		T result;

		if (!Buffer::trait::linear)
			type = Nearest;

		switch (type)
		{
		case Nearest:
		{
			size_t i0 = static_cast<size_t>(index), i1 = i0 + 1;
			fsize_t ir = index - i0;
			i1 = std::min(i1, std::deque<T>::size() - 1);
			result = (ir < fsize_t(0.5)) ?
				std::deque<T>::at(i0) :
				std::deque<T>::at(i1);
			break;
		}
		case Linear:
		case Spline:
		{
			size_t i0 = static_cast<size_t>(index), i1 = i0 + 1;
			fsize_t ir = index - i0;
			i1 = std::min(i1, std::deque<T>::size() - 1);
			result = Buffer::trait::mix(std::deque<T>::at(i0),
				std::deque<T>::at(i1),
				ir);
			break;
		}
		}

		return result;
	}

	inline void to(std::vector<T>& vector) const override {
		vector.reserve(std::deque<T>::size());
		std::copy(std::deque<T>::cbegin(),
			std::deque<T>::cend(),
			vector.begin());
	}

	inline T in(const T& input) override {
		return ProcessChain<T>::in(input);
	}

	inline void fill(const T& value) override {
		std::fill(std::deque<T>::begin(),
			std::deque<T>::end(),
			value);
	}

protected:
	std::string m_name;

	inline T process(const T& input) override {
		std::deque<T>::pop_back();
		std::deque<T>::emplace_front(input);
		return input;
	}
};

template<typename T>
std::ostream& operator<<(std::ostream& a, const Buffer<T>& b) {
	std::stringstream ss;
	ss << b.name() << "[" << b.size() << "](" << b.front();
	auto it = b.cbegin();
	while ((++it) != b.cend()) {
		ss << ", " << *it;
	}
	ss << ")";
	a << ss.str();
	return a;
}

template<typename T>
std::string trace(const Buffer<T>& head) {
	struct _buf_info {
		Buffer<T>* buffer;
		size_t prev, type, endpos;
	};

	std::stringstream ss;
	std::string header, s;
	std::vector<size_t> stack;
	std::vector<_buf_info> list;
	Buffer<T> *curr = const_cast<Buffer<T>*>(&head);
	ProcessChain<T> *succ;
	size_t endpos = 0;

	stack.emplace_back(0);
	list.push_back({ curr, 0, 0, 0 });
	while (!stack.empty()) {
		auto idx = stack.back();
		auto &buf = list[idx];
		stack.pop_back();

		curr = buf.buffer;
		switch (buf.type & 0x0f) {
		case 0:
		{
			ss << curr->name() << "[" << curr->size() << "]";
			break;
		}
		case 1:
		{
			if ((buf.type & 0xf0) > 0)
				ss << " ->...-> " << curr->name() << "[" << curr->size() << "]";
			else
				ss << " -> " << curr->name() << "[" << curr->size() << "]";
			break;
		}
		case 2:
		{
			ss << std::endl;
			header += ss.str(); ss.str("");
			ss << std::string(list[buf.prev].endpos - 1, ' ');
			if ((buf.type & 0xf0) > 0)
				ss << '/';
			else
				ss << '|';
			ss << std::string(endpos - list[buf.prev].endpos, '-');
			endpos = 0;
			ss << "--> " << curr->name() << "[" << curr->size() << "]";
			break;
		}
		}
		s = ss.str(); header += s; ss.str("");
		endpos += s.length();
		buf.endpos = endpos;

		size_t flag = 0;
		succ = curr->next();
		// skip non-buffers in simbling
		while (succ != nullptr && dynamic_cast<Buffer<T>*>(succ) == nullptr) {
			succ = succ->next();
			flag = 0x10; // skipped
		}
		if (succ != nullptr) {
			stack.emplace_back(list.size());
			list.push_back({ static_cast<Buffer<T>*>(succ), buf.prev, 2 | flag, 0 });
		}

		succ = curr->first();
		// skip non-buffers through chain
		while (succ != nullptr && dynamic_cast<Buffer<T>*>(succ) == nullptr) {
			succ = succ->first();
			flag = 0x20;
		}
		if (succ != nullptr) {
			stack.emplace_back(list.size());
			list.push_back({ static_cast<Buffer<T>*>(succ), idx, 1 | flag, 0 });
		}
	}

	s = header;
	std::sort(list.begin(), list.end(),
		[](const _buf_info& a, const _buf_info& b) {
		return a.endpos < b.endpos;
	});
	auto max_size = std::max_element(list.cbegin(), list.cend(),
		[](const _buf_info& a, const _buf_info& b) {
		return a.buffer->size() < b.buffer->size();
	})->buffer->size();
	ss << std::endl << std::string(endpos, '_');

	for (size_t i = 0; i < max_size; ++i) {
		ss << std::endl;
		size_t endpos = 0;
		for (auto &buf : list) {
			ss << std::setw(buf.endpos - endpos);
			endpos = buf.endpos;
			if (buf.buffer->size() > i)
				ss << buf.buffer->at(i);
			else
				ss << '-';
		}
	}
	s += ss.str();

	return s;
}

template<typename T>
class NuBuffer :
	public Buffer<T>
{
public:
	NuBuffer(size_t size,
		Buffer<time_t>* timeRef,
		ProcessChain<T>* parent = nullptr) :
		Buffer<T>(size, parent)
	{
		ASSERT(size > 1);
		setTimeRef(timeRef);
	}

	//inline std::vector<TimeValuePair<T>>& operator>>(
	//		std::vector<TimeValuePair<T>>& vector) const {
	//	to(vector);
	//	return vector;
	//}

	virtual inline std::string name() const override {
		return this->m_name.empty() ? "NuBuffer" : this->m_name;
	}

	inline time_t time() const {
		ASSERT(m_timeRef != nullptr);
		return m_timeRef->front();
	}

	inline time_t span() const {
		ASSERT(m_timeRef != nullptr);
		return m_timeRef->front() - m_timeRef->back();
	}

	inline fsize_t seek(time_t time, SampleType type = Nearest) const {
		ASSERT(m_timeRef != nullptr);
		size_t l = 0, r = m_timeRef->size() - 1, m = l;
		while (l + 1 < r) {
			m = (l + r) >> 1;
			if (m_timeRef->at(m) < time)
				r = m;
			else
				l = m;
		}
		fsize_t result;

		switch (type)
		{
		case Nearest:
		{
			l = r > 0 ? r - 1 : 0;
			time_t t0 = m_timeRef->at(l), t1 = m_timeRef->at(r);
			result = static_cast<fsize_t>((time - t0 < t1 - time) ? l : r);
			break;
		}
		case Linear:
		case Spline:
		{
			l = r > 0 ? r - 1 : 0;
			time_t t0 = m_timeRef->at(l), t1 = m_timeRef->at(r);
			fsize_t ir = (time - t0) / (t1 - t0);
			ir = clamp(ir, 0, 1);
			result = static_cast<fsize_t>(l) + ir;
			break;
		}
		}

		return result;
	}

	inline T atTime(time_t time, SampleType type = Nearest) const {
		return Buffer<T>::sample(seek(time, type), type);
	}

	inline void to(std::vector<TimeValuePair<T>>& vector) const {
		ASSERT(m_timeRef != nullptr);
		vector.clear();
		for (size_t i = 0; i < std::deque<T>::size(); ++i)
			vector.emplace_back(TimeValuePair<T>(m_timeRef->at(i),
				std::deque<T>::at(i)));
	}

	inline Buffer<time_t>* timeRef() const { return m_timeRef; }
	inline void setTimeRef(Buffer<time_t>* timeRef) {
		ASSERT(timeRef != nullptr);
		ASSERT(timeRef->size() >= std::deque<T>::size());
		m_timeRef = timeRef;
	}

	inline void setParent(NuBuffer* parent) {
		ProcessChain<T>::setParent(parent);
		if (parent != nullptr) {
			m_timeRef = parent->m_timeRef;
		}
	}

protected:
	Buffer<time_t> *m_timeRef;
};

template<typename T>
std::ostream& operator<<(std::ostream& a, const NuBuffer<T>& b) {
	ASSERT(b.timeRef() != nullptr);
	std::stringstream ss;
	ss << b.name() << "[" << b.size() << "](("
		<< b.time() << "," << b.front() << ")";
	auto it = b.timeRef()->cbegin(), iv = b.cbegin();
	while ((++iv) != b.cend()) {
		ss << ", (" << *(++it) << "," << *iv << ")";
	}
	ss << ")";
	a << ss.str();
	return a;
}

template<typename T>
std::string trace(const NuBuffer<T>& head) {
	struct _buf_info {
		Buffer<T>* buffer;
		size_t prev, type, endpos;
	};

	std::stringstream ss;
	std::string header, s;
	std::vector<size_t> stack;
	std::vector<_buf_info> list;
	Buffer<T> *curr = const_cast<Buffer<T>*>(
		static_cast<const Buffer<T>*>(&head));
	ProcessChain<T> *succ;

	auto timeRef = head.timeRef();
	ASSERT(timeRef != nullptr);

	ss << "        Time | ";
	s = ss.str(); header += s; ss.str("");
	size_t padding = s.length();
	size_t endpos = padding;

	stack.emplace_back(0);
	list.push_back({ curr, 0, 0, 0 });
	while (!stack.empty()) {
		auto idx = stack.back();
		auto &buf = list[idx];
		stack.pop_back();

		curr = buf.buffer;
		switch (buf.type & 0x0f) {
		case 0:
		{
			ss << curr->name() << "[" << curr->size() << "]";
			break;
		}
		case 1:
		{
			if ((buf.type & 0xf0) > 0)
				ss << " ->...-> " << curr->name() << "[" << curr->size() << "]";
			else
				ss << " -> " << curr->name() << "[" << curr->size() << "]";
			break;
		}
		case 2:
		{
			ss << std::endl;
			header += ss.str(); ss.str("");
			ss << std::string(list[buf.prev].endpos - 1, ' ');
			if ((buf.type & 0xf0) > 0)
				ss << '/';
			else
				ss << '|';
			ss << std::string(endpos - list[buf.prev].endpos, '-');
			endpos = 0;
			ss << "--> " << curr->name() << "[" << curr->size() << "]";
			break;
		}
		}
		s = ss.str(); header += s; ss.str("");
		endpos += s.length();
		buf.endpos = endpos;

		size_t flag = 0;
		succ = curr->next();
		// skip non-buffers in simbling
		while (succ != nullptr && dynamic_cast<Buffer<T>*>(succ) == nullptr) {
			succ = succ->next();
			flag = 0x10; // skipped
		}
		if (succ != nullptr) {
			stack.emplace_back(list.size());
			list.push_back({ static_cast<Buffer<T>*>(succ), buf.prev, 2 | flag, 0 });
		}

		succ = curr->first();
		// skip non-buffers through chain
		while (succ != nullptr && dynamic_cast<Buffer<T>*>(succ) == nullptr) {
			succ = succ->first();
			flag = 0x20;
		}
		if (succ != nullptr) {
			stack.emplace_back(list.size());
			list.push_back({ static_cast<Buffer<T>*>(succ), idx, 1 | flag, 0 });
		}
	}

	s = header;
	std::sort(list.begin(), list.end(),
		[](const _buf_info& a, const _buf_info& b) {
		return a.endpos < b.endpos;
	});
	auto max_size = std::max_element(list.cbegin(), list.cend(),
		[](const _buf_info& a, const _buf_info& b) {
		return a.buffer->size() < b.buffer->size();
	})->buffer->size();
	ASSERT(max_size <= timeRef->size());
	ss << std::endl << std::string(endpos, '_');

	for (size_t i = 0; i < max_size; ++i) {
		ss << std::endl;
		ss << std::setw(padding - 3) << timeRef->at(i) << std::string(3, ' ');
		size_t endpos = padding;
		for (auto &buf : list) {
			ss << std::setw(buf.endpos - endpos);
			endpos = buf.endpos;
			if (buf.buffer->size() > i)
				ss << buf.buffer->at(i);
			else
				ss << '-';
		}
	}
	s += ss.str();

	return s;
}

}

#endif // BUFFER_H
