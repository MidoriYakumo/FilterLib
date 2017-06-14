
#include <iostream>
#include <algorithm>

#include <QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QtCharts>

#include "buffer.h"
#include "filter.h"

using namespace QtCharts;
using namespace std;
using namespace FilterLib;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QWidget widget;
	widget.resize(800, 600);
	QBoxLayout layout(QBoxLayout::Down, &widget);
	QChart chart;
	chart.setTitle("Audio waveform");
	auto chartView = new QChartView(&chart, &widget);
	layout.addWidget(chartView, 1);

	QtCharts::QValueAxis axisX(&chart), axisY(&chart);
	axisX.setTitleText("Time");
	axisY.setTitleText("Value");

	{
		Buffer<float> t0(16); // time
		NuBuffer<float> b0(16, &t0);
		b0.setName("Input");

		NuBuffer<float> i1(16, &t0, &b0), o1(16, &t0);
		Comparator<float> f1(0, &i1); o1.ProcessChain::setParent(&f1);
		o1.setName("Comparator");
		f1.setThreshold(-5, 5);

		for (size_t i = 0; i < b0.size(); ++i) {
			float(i) >> t0;
			(float(i) * sinf(i)) >> b0;
		}

		{
			auto buffer = &b0;
			auto series = new QLineSeries;
			series->setName(QString::fromStdString(buffer->name()));
			chart.addSeries(series);
			chart.setAxisX(&axisX, series);
			chart.setAxisY(&axisY, series);

			QVector<QPointF> points;
			for (size_t i = 0; i<buffer->size(); ++i)
				points.append({qreal(buffer->timeRef()->at(i)),
							   qreal(buffer->at(i))});
			series->replace(points);
		}

		{
			auto buffer = &o1;
			auto series = new QLineSeries;
			series->setName(QString::fromStdString(buffer->name()));
			chart.addSeries(series);
			chart.setAxisX(&axisX, series);
			chart.setAxisY(&axisY, series);

			QVector<QPointF> points;
			for (size_t i = 0; i<buffer->size(); ++i)
				points.append({qreal(buffer->timeRef()->at(i)),
							   qreal(buffer->at(i))});
			series->replace(points);
		}

		axisX.setRange(qreal(t0.back()), qreal(t0.front()));
		axisY.setRange(-9, 9);
	}

	widget.show();
	return a.exec();
}
