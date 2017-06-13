#ifndef FILTER_H
#define FILTER_H

#include "buffer.h"

#include <algorithm>

namespace FilterLib {

template<typename ValueT>
struct Filter_T {
	typedef ValueT ValueType;
	static constexpr ValueType zero = ValueType();
	static constexpr ValueType unit = ValueType();
};

template<>
struct Filter_T<int> {
	typedef int ValueType;
	static constexpr ValueType zero = 0;
	static constexpr ValueType unit = 1;
};

template<>
struct Filter_T<float> {
	typedef float ValueType;
	static constexpr ValueType zero = 0.f;
	static constexpr ValueType unit = 1.f;
};

template<typename T>
class AbstractFilter {
public:
	typedef Filter_T<T> trait;

	AbstractFilter() :
		m_out(trait::zero)
	{

	}

	virtual ~AbstractFilter() { }

	inline T out() const { return m_out; }

	virtual T in(const T& input) = 0;

protected:
	T m_out;
};

template<typename T>
class Filter :
	public ProcessChain<T>,
	public AbstractFilter<T>
{
public:
	Filter(ProcessChain<T>* parent = nullptr) :
		ProcessChain<T>(parent),
		AbstractFilter<T>()
	{

	}

	virtual ~Filter() { }

	inline T out() const { return this->m_out; }

	inline T in(const T& input) override {
		return ProcessChain<T>::in(input);
	}

protected:
	virtual inline T process(const T& input) override {
		this->m_out = input;
		return this->m_out;
	}
};

template<typename T>
class NuFilter :
	public Filter<T>
{
public:
	NuFilter(Buffer<time_t>* timeRef,
			 ProcessChain<T>* parent = nullptr) :
		Filter<T>(parent)
	{
		setTimeRef(timeRef);
	}

	virtual ~NuFilter() { }

	inline T out() const { return this->m_out; }

	inline time_t time() const {
		ASSERT(m_timeRef != nullptr);
		return m_timeRef->front();
	}

	inline void setTimeRef(Buffer<time_t>* timeRef) {
		ASSERT(timeRef != nullptr);
		m_timeRef = timeRef;
	}

	inline void setParent(NuBuffer<T>* parent) {
		ASSERT(ProcessChain<T>::m_parent == nullptr);
		if (parent != nullptr) {
			ProcessChain<T>::m_parent = static_cast<ProcessChain<T>*>(parent);
			ProcessChain<T>::m_simbling = parent->m_child;
			ProcessChain<T>::m_index = (ProcessChain<T>::m_simbling == nullptr) ?
				0 :
				ProcessChain<T>::m_simbling->m_index + 1;
			m_timeRef = parent->m_timeRef;
			parent->m_child = this;
		}
	}

	inline T in(const T& input) override {
		return ProcessChain<T>::in(input);
	}

protected:
	Buffer<time_t> *m_timeRef;

	virtual inline T process(const T& input) override {
		this->m_out = input;
		return this->m_out;
	}
};


template <typename T>
class Comparator :
	public Filter<T>
{
public:
	Comparator(const T& initial = Comparator<T>::trait::zero,
			   ProcessChain<T>* parent = nullptr) :
		Filter<T>(parent),
		m_low(Comparator<T>::trait::zero),
		m_high(Comparator<T>::trait::zero)
	{
		this->m_out = initial;
	}

	inline void setThreshold(const T& threshold) {
		m_low = threshold;
		m_high = threshold;
	}

	inline void setThreshold(const T& low, const T& high) {
		m_low = low;
		m_high = high;
	}

protected:
	T m_low, m_high;

	inline T process(const T& input) override {
		if (input < m_low)
			this->m_out = Comparator<T>::trait::zero;
		else if (input > m_high)
			this->m_out = Comparator<T>::trait::unit;
		return this->m_out;
	}
};

template<typename T>
class HoldHigh :
	public Filter<T>
{
public:
	HoldHigh(size_t size, ProcessChain<T>* parent = nullptr) :
		Filter<T>(),
		m_input(size, parent)
	{
		ProcessChain<T>::setParent(&m_input);
	}

protected:
	Buffer<T> m_input;

	inline T process(const T& input) override {
		(void)(input);
		this->m_out = *(std::max_element(m_input.cbegin(), m_input.cend()));
		return this->m_out;
	}

};

template<typename T>
class HoldLow :
	public Filter<T>
{
public:
	HoldLow(size_t size, ProcessChain<T>* parent = nullptr) :
		Filter<T>(),
		m_input(size, parent)
	{
		ProcessChain<T>::setParent(&m_input);
	}

protected:
	Buffer<T> m_input;

	inline T process(const T& input) override {
		this->m_out = *(std::min_element(m_input.cbegin(), m_input.cend()));
		return this->m_out;
	}

};

template <typename T>
class Limiter :
	public Filter<T>
{
public:
	Limiter(ProcessChain<T>* parent = nullptr) :
		Filter<T>(parent),
		m_low(Limiter<T>::trait::zero),
		m_high(Limiter<T>::trait::unit)
	{

	}

	inline void setLimit(const T& low, const T& high) {
		m_low = low;
		m_high = high;
	}

protected:
	T m_low, m_high;

	inline T process(const T& input) override {
		this->m_out = (input < m_low) ? m_low : (input > m_high) ? m_high : input;
		return this->m_out;
	}

};

template<typename T>
class MidAntiJitter :
	public Filter<T>
{
public:
	MidAntiJitter(size_t size, ProcessChain<T>* parent = nullptr) :
		Filter<T>(),
		m_input(size, parent)
	{
		ProcessChain<T>::setParent(&m_input);
	}

protected:
	Buffer<T> m_input;
	std::vector<T> m_tmpBuf;

	inline T process(const T& input) override {
		(void)(input);
		m_input.to(m_tmpBuf);
		std::sort(m_tmpBuf.begin(), m_tmpBuf.end());
		this->m_out = m_tmpBuf[m_input.size() / 2];
		return this->m_out;
	}

};

//template <>
class HistAntiJitter :
	public Filter<float>
{
public:
	HistAntiJitter(size_t size, size_t histSize,
				   float tMin, float tMax,
				   float margin = 0.05f, ProcessChain<float>* parent = nullptr) :
		Filter<float>(),
		m_histSize(histSize),
		m_margin(size_t(size * margin)),
		m_tMin(tMin),
		m_tMax(tMax),
		m_tSpan(tMax - tMin),
		m_histogram(histSize, 0),
		m_input(size, parent)
	{
		ProcessChain<float>::setParent(&m_input);
		m_histogram[which(0)] = size;
	}

protected:
	size_t m_histSize, m_margin;
	float m_tMin, m_tMax, m_tSpan;
	std::vector<size_t> m_histogram;
	Buffer<float> m_input;

	inline float process(const float& input) override {
		auto hLast = which(m_input.back());
		auto hCurrent = which(input);
		ASSERT(m_histogram[hLast] > 0);
		m_histogram[hLast]--;
		m_histogram[hCurrent]++;

		size_t hLow = 0, hHigh = m_histSize - 1, acc;
		acc = 0;
		while (acc <= m_margin) {
			acc += m_histogram[hLow++];
		}
		hLow--;
		acc = 0;
		while (acc <= m_margin) {
			acc += m_histogram[hHigh--];
		}
		hHigh++;

		this->m_out = input;
		if (hCurrent < hLow) {
			this->m_out = what(hLow);
		} else if (hCurrent > hHigh) {
			this->m_out = what(hHigh + 1);
		}

		return this->m_out;
	}

	inline float what(size_t h) const {
		return m_tSpan * h / (m_histSize - 1) + m_tMin;
	}

	inline size_t which(const float& value) const {
		float h = (value - m_tMin) / m_tSpan;
		h = (h < 0) ? 0 : h;
		h = (h > 1) ? 1 : h;
		h *= float(m_histSize - 1);
		return size_t(h);
	}
};

















//template<typename T>
//class FIRFilter :
//		public Buffer<T>
//{
//public:
//	explicit FIRFilter(size_t size, Buffer<T>* parent = nullptr) :
//		Buffer<T>(size, parent),
//		m_input(size),
//		m_coeff(new T[size]),
//		m_scale(1)
//	{

//	}

//	virtual ~FIRFilter() {
//		delete this->m_coeff;
//	}

//	virtual inline void setCoeff(const T coeff[], T scale = 1) {
//		memcpy(this->m_coeff, coeff, sizeof(T) * this->m_size);
//		m_scale = scale;
//	}

//	virtual inline T step(const T& value) {
//		this->shift();
//		this->write(m_input.step(value));

//		T result = 0;
//		for (size_t i = 0; i<this->m_size; ++i)
//			result += this->m_input[i] * this->m_coeff[i];
//		result /= this->m_scale;

//		this->store(result);

//		if (this->m_simbling != nullptr)
//			static_cast<Buffer<T>*>(this->m_simbling)->step(value);
//		if (this->m_child != nullptr)
//			static_cast<Buffer<T>*>(this->m_child)->step(result);

//		return result;
//	}

//protected:
//	Buffer<T> m_input;
//	T *m_coeff, m_scale;
//};

//template<typename T>
//class IIRFilter :
//		public Buffer<T>
//{
//public:
//	explicit IIRFilter(size_t size, Buffer<T>* parent = nullptr) :
//		Buffer<T>(size, parent),
//		m_input(size),
//		m_coeffA(new T[size]),
//		m_coeffB(new T[size])
//	{
//		this->m_input.fill(0);
//	}

//	virtual ~IIRFilter() {
//		delete this->m_coeffB;
//		delete this->m_coeffA;
//	}

//	virtual inline void setCoeff(const T coeffA[], const T coeffB[]) {
//		memcpy(this->m_coeffA, coeffA, sizeof(T) * this->m_size);
//		memcpy(this->m_coeffB, coeffB, sizeof(T) * this->m_size);
//	}

//	virtual inline T step(const T& value) {
//		this->shift();
//		this->write(this->m_input.step(value));

//		T result = 0;
//		for (size_t i = 0; i<this->m_size; ++i)
//			result += this->m_input[i] * this->m_coeffB[i];
//		for (size_t i = this->m_size - 1; i>0; --i)
//			result -= this->at(i) * this->m_coeffA[i];
//		result /= this->m_coeffA[0];

//		this->store(result);

//		if (this->m_simbling != nullptr)
//			static_cast<Buffer<T>*>(this->m_simbling)->step(value);
//		if (this->m_child != nullptr)
//			static_cast<Buffer<T>*>(this->m_child)->step(result);

//		return result;
//	}

//protected:
//	Buffer<T> m_input;
//	T *m_coeffA, *m_coeffB;
//};

}

#endif // FILTER_H
