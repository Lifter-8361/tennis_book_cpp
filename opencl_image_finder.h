#pragma once

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QVector>
#include <CL/opencl.h>

class OpenCLImageFinder : public QObject
{
	Q_OBJECT

public:
	explicit OpenCLImageFinder(QObject* parent = nullptr);
	~OpenCLImageFinder();

	bool initializeOpenCL();
	QPoint findFirstMatchMinimal(const QImage& source, const QImage& target,
		double requiredSimilarity = 0.95);

	QString getDeviceInfo() const;

private:
	bool compileKernel();
	void cleanupOpenCL();
	void printDeviceInfo() const;

	QImage convertToGrayscale(const QImage& image);
	QVector<float> convertGrayscaleToFloatArray(const QImage& grayscaleImage);

	cl_context context;
	cl_device_id device;
	cl_command_queue queue;
	cl_program program;
	cl_kernel kernel;
	bool isInitialized;
};
