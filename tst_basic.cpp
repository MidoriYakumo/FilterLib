
#include <iostream>
#include <algorithm>

#include "buffer.h"
#include "filter.h"

using namespace std;
using namespace FilterLib;

int main(int argc, char *argv[])
{
	{
		cout << "Trait:" << endl;
		cout << Buffer<int>::trait::linear << endl;
		cout << Buffer<void*>::trait::linear << endl;
		cout << Buffer<void*>::trait::zero << endl;
		cout << endl;
	}

	{
		cout << "Chained action:" << endl;
		Buffer<int> b1(1), b2(2, &b1), b3(3, &b2), b4(4, &b2);
		Buffer<int> b5(5, &b2), b6(6, &b4);
		b1.setName("one");
		b2.setName("two");
		cout << b3.index() << endl;
		cout << b1 << ' ' << b2 << ' ' << b3 << ' ' << b4 << endl;
		b1 << 4 << 1 << 3;
		cout << b1 << ' ' << b2 << ' ' << b3 << ' ' << b4 << endl;
		cout << trace(b1) << endl;
		cout << endl;
	}

	{
		cout << "Operator overload:" << endl;
		Buffer<float> b1(1), b2(2), b3(3), b4(4);
		b1(1);
		b4 << b1 << 2.f;
		b3(b2(b4));
		b3 >> b1 >> b2 >> b4;
		cout << b1 << ' ' << b2 << ' ' << b3 << ' ' << b4 << endl;
		cout << endl;
	}

	{
		cout << "Sampling:" << endl;
		Buffer<int> b1(8);
		b1 << 3 << 1 << 4 << 1 << 5 << 9 << 2 << 6;
		cout << b1 << endl;
		cout << b1.sample(4.0f) << ' ' << b1.sample(4.3f) <<
				' ' << b1.sample(4.6f, SampleType::Nearest) <<
				' ' << b1.sample(4.9f) << endl;
		cout << endl;
	}

	{
		cout << "Iterating:" << endl;
		Buffer<int> b1(8);
		b1 << 3 << 1 << 4 << 1 << 5 << 9 << 2 << 6;
		cout << b1 << endl;
		std::sort(b1.begin(), b1.end());
		cout << b1 << endl;
		cout << endl;
	}

	{
		cout << "Nonuniform timing sampling:" << endl;
		Buffer<float> t0(16); // time
		NuBuffer<float> b1(8, &t0), b2(12, &t0, &b1), b3(8, &t0, &b1), b4(10, &t0, &b2);
		t0 << 0.f << .1f << .5f << .9f << 1.2f << 2.9f << 5.6f << 8.0f;
		b1 << 3.f << 1.f << 4.f << 1.f << 5.f << 9.f << 2.f << 6.f;
		cout << b1.span() << endl;
		cout << b1 << endl;
		cout << b1.atTime(1.8f, SampleType::Linear) << endl;
		cout << trace(b1) << endl;
		cout << endl;
	}

	{
		cout << "Filters:" << endl;
		Buffer<float> t0(16); // time
		NuBuffer<float> b0(16, &t0);
		b0.setName("Input");

		NuBuffer<float> i1(16, &t0, &b0), o1(16, &t0);
		Comparator<float> f1(0, &i1); o1.ProcessChain::setParent(&f1);
		o1.setName("Comparator");
		f1.setThreshold(-5, 5);

		NuBuffer<float> i2(16, &t0, &b0), o2(16, &t0);
		HoldHigh<float> f2(6, &i2); o2.ProcessChain::setParent(&f2);
		o2.setName("HoldHigh");

		NuBuffer<float> i3(16, &t0, &b0), o3(16, &t0);
		Limiter<float> f3(&i3); o3.ProcessChain::setParent(&f3);
		o3.setName("Limiter");
		f3.setLimit(-5, 5);

		NuBuffer<float> i4(16, &t0, &b0), o4(16, &t0);
		MidAntiJitter<float> f4(6, &i4); o4.ProcessChain::setParent(&f4);
		o4.setName("MidAntiJitter");

		NuBuffer<float> i5(16, &t0, &b0), o5(16, &t0);
		HistAntiJitter f5(6, 15, -10, 10, 0.05f, &i5); o5.ProcessChain::setParent(&f5);
		o5.setName("HistAntiJitter");

		for (size_t i = 0; i < b0.size(); ++i) {
			(float(i) * sinf(i)) >> t0 >> b0;
		}

		cout << trace(b0) << endl;
		cout << endl;
	}

	return 0;
}
