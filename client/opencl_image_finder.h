#pragma once

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QVector>
#include <CL/opencl.h>

#include "geometry_area.h"

class OpenCLImageFinder final
	: public QObject
{
	Q_OBJECT

public:
	explicit OpenCLImageFinder(QObject* parent = nullptr);
	~OpenCLImageFinder() override;

	bool InitializeOpenCL();
	QPoint FindFirstMatchMinimal(const QImage& source, const QImage& target, double requiredSimilarity = 0.95);

	QString GetDeviceInfo() const;

	void SetParams(const geometry_area& area, int monitor_number);

Q_SIGNALS:

	void Failed();
	void Succeed();

public Q_SLOT:

	void OnStartClicked();
	void OnStopClicked();

private:
	bool CompileKernel();
	void CleanupOpenCL();
	void PrintDeviceInfo() const;

	QImage ConvertToGrayscale(const QImage& image);
	QVector<float> ConvertGrayscaleToFloatArray(const QImage& grayscaleImage);

private:

	cl_context context_;
	cl_device_id device_;
	cl_command_queue queue_;
	cl_program program_;
	cl_kernel kernel_;
	bool is_initialized_;

	geometry_area detect_area_ = {};
	int monitor_number_ = 0;
};
