#include "opencl_image_finder.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>

#if DEBUG_GRAYSCALE
#include <QLabel>
#endif

#define CL_TARGET_OPENCL_VERSION 120

#define DEBUG_GRAYSCALE 0

OpenCLImageFinder::OpenCLImageFinder(QObject* parent)
	: QObject(parent)
	, context_(nullptr)
	, device_(nullptr)
	, queue_(nullptr)
	, program_(nullptr)
	, kernel_(nullptr)
	, is_initialized_(false)
{
}

OpenCLImageFinder::~OpenCLImageFinder()
{
	CleanupOpenCL();
}

void OpenCLImageFinder::CleanupOpenCL()
{
	if (kernel_)
	{
		clReleaseKernel(kernel_);
	}

	if (program_)
	{
		clReleaseProgram(program_);
	}

	if (queue_)
	{
		clReleaseCommandQueue(queue_);
	}

	if (context_)
	{
		clReleaseContext(context_);
	}

	kernel_ = nullptr;
	program_ = nullptr;
	queue_ = nullptr;
	context_ = nullptr;
	is_initialized_ = false;
}

bool OpenCLImageFinder::InitializeOpenCL()
{
	if (is_initialized_)
	{
		return true;
	}

	cl_int err;
	cl_platform_id platform;

	err = clGetPlatformIDs(1, &platform, nullptr);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Ошибка получения платформы OpenCL : ") << err;
		return false;
	}

	cl_device_type deviceType = CL_DEVICE_TYPE_GPU;
	err = clGetDeviceIDs(platform, deviceType, 1, &device_, nullptr);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("GPU не найден, пробуем CPU...");
		deviceType = CL_DEVICE_TYPE_CPU;
		err = clGetDeviceIDs(platform, deviceType, 1, &device_, nullptr);
		if (err != CL_SUCCESS)
		{
			qWarning() << QString::fromUtf8("Ошибка получения устройства OpenCL:") << err;
			return false;
		}
	}

	context_ = clCreateContext(nullptr, 1, &device_, nullptr, nullptr, &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Ошибка создания контекста:") << err;
		return false;
	}

	queue_ = clCreateCommandQueue(context_, device_, 0, &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Ошибка создания очереди команд:") << err;
		CleanupOpenCL();
		return false;
	}
	PrintDeviceInfo();

	if (!CompileKernel())
	{
		CleanupOpenCL();
		return false;
	}

	PrintDeviceInfo();
	is_initialized_ = true;
	return true;
}

bool OpenCLImageFinder::CompileKernel()
{
	const char* kernel_source = R"(
    __kernel void findFirstMatchMinimal(
        __global const float* source,
        __global const float* target,
        volatile __global int* output,          // [found, x, y]
        const int sourceWidth,
        const int sourceHeight,
        const int targetWidth,
        const int targetHeight,
        const float requiredSimilarity)
    {
        int x = get_global_id(0);
        int y = get_global_id(1);
        
        // Проверяем, не найден ли уже результат (правильный atomic load)
        int found = atomic_add(&output[0], 0); // Atomic read
        if (found != 0) {
            return;
        }
        
        if (x >= sourceWidth - targetWidth || y >= sourceHeight - targetHeight) {
            return;
        }
        
        float match = 0.0f;
        int totalPixels = targetWidth * targetHeight;
        
        for (int ty = 0; ty < targetHeight; ty++) {
            for (int tx = 0; tx < targetWidth; tx++) {
                float sourceVal = source[(y + ty) * sourceWidth + (x + tx)];
                float targetVal = target[ty * targetWidth + tx];
                
				//printf("source: %f ; target: %f \n", sourceVal, targetVal);

                // Быстрое сравнение с допуском
                if (fabs(sourceVal - targetVal) < 0.03f) {
					//printf("x: %d y: %d ; f: %f\n", x, y, fabs(sourceVal - targetVal));
                    match += 1.0f;
                }
            }
            
            // Проверяем флаг после каждой строки
            found = atomic_add(&output[0], 0); // Atomic read
            if (found != 0) return;
            
            // Ранний выход если уже не можем достичь requiredSimilarity
            float currentSimilarity = match / ((ty + 1) * targetWidth);
            float maxPossible = currentSimilarity + (float)(targetHeight - ty - 1) * targetWidth / totalPixels;
            if (maxPossible < requiredSimilarity) {
                break;
            }
        }
        
        float finalSimilarity = match / (float)(totalPixels);
		//printf("x: %d y: %d ; final: %f ; required: %f\n", x, y, finalSimilarity, requiredSimilarity);
        if (finalSimilarity >= requiredSimilarity) {
            int oldValue = atomic_cmpxchg(&output[0], 0, 1);
            if (oldValue == 0) {
                // Первый поток, который нашел - сохраняет координаты
                atomic_xchg(&output[1], x);
                atomic_xchg(&output[2], y);
            }
        }
    }
    )";

	cl_int err;
	program_ = clCreateProgramWithSource(context_, 1, &kernel_source, nullptr, &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Create program error : ") << err;
		return false;
	}

	err = clBuildProgram(program_, 1, &device_, nullptr, nullptr, nullptr);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Compile program error. kernel : ") << err;

		size_t logSize;
		clGetProgramBuildInfo(program_, device_, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
		QVector<char> log(logSize);
		clGetProgramBuildInfo(program_, device_, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
		qWarning() << QString::fromUtf8("Compilation log : ") << log.data();

		return false;
	}

	kernel_ = clCreateKernel(program_, "findFirstMatchMinimal", &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Creation error. kernel : ") << err;
		return false;
	}

	return true;
}

void OpenCLImageFinder::PrintDeviceInfo() const
{
	if (!device_) return;

	char deviceName[128] = { 0 };
	clGetDeviceInfo(device_, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);

	cl_ulong globalMemSize;
	clGetDeviceInfo(device_, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, nullptr);

	size_t maxWorkGroupSize;
	clGetDeviceInfo(device_, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr);

	qDebug() << QString::fromUtf8("OpenCL Device : ") << deviceName;
	qDebug() << QString::fromUtf8("Global Memory : ") << globalMemSize / (1024 * 1024) << QString::fromUtf8(" MB");
	qDebug() << QString::fromUtf8("Max Work Group Size : ") << maxWorkGroupSize;
}

QImage OpenCLImageFinder::ConvertToGrayscale(const QImage& image)
{
	if (image.isNull())
	{
		return QImage();
	}
	else if (image.format() == QImage::Format_Grayscale8)
	{
		return image;
	}

	return image.convertToFormat(QImage::Format_Grayscale8);
}

QVector<float> OpenCLImageFinder::ConvertGrayscaleToFloatArray(const QImage& grayscaleImage)
{
	if (grayscaleImage.isNull() || grayscaleImage.format() != QImage::Format_Grayscale8)
	{
		return QVector<float>();
	}

	const int width = grayscaleImage.width();
	const int height = grayscaleImage.height();
	QVector<float> array(width * height);

	for (int y = 0; y < height; y++)
	{
		const uchar* scanLine = grayscaleImage.constScanLine(y);
		for (int x = 0; x < width; x++)
		{
			float float_v = static_cast<float>(scanLine[x]);
			array[y * width + x] = static_cast<float>(scanLine[x]) / 255.0f;
		}
	}

	return array;
}

QPoint OpenCLImageFinder::FindFirstMatchMinimal(const QImage& source, const QImage& target,
	double requiredSimilarity)
{
	if (!is_initialized_ && !InitializeOpenCL())
	{
		qWarning() << QString::fromUtf8("OpenCL is not initialized.");
		return QPoint(-1, -1);
	}

	if (source.isNull() || target.isNull())
	{
		qWarning() << QString::fromUtf8("images not loaded.");
		return QPoint(-1, -1);
	}

	QElapsedTimer timer;
	timer.start();

	// Конвертируем в grayscale
	QImage source_grayscale_img = ConvertToGrayscale(source);
	QImage target_grayscale_img = ConvertToGrayscale(target);

	if (source_grayscale_img.isNull() || target_grayscale_img.isNull())
	{
		qWarning() << QString::fromUtf8("Convert to grayscale error!");
		return QPoint(-1, -1);
	}

#if DEBUG_GRAYSCALE
	auto display_image = [](const QString& text, const QImage& img)
		{
			QLabel* lbl = new QLabel;
			lbl->setWindowTitle(text);
			lbl->setPixmap(QPixmap::fromImage(img));
			lbl->show();
		};

	display_image("sourceGray", source_grayscale_img);
	display_image("targetGray", target_grayscale_img);
	display_image("source", source);
	display_image("target", target);
#endif

	const int sourceWidth = source_grayscale_img.width();
	const int sourceHeight = source_grayscale_img.height();
	const int targetWidth = target_grayscale_img.width();
	const int targetHeight = target_grayscale_img.height();

	if (targetWidth > sourceWidth || targetHeight > sourceHeight)
	{
		qWarning() << QString::fromUtf8("Target image greater than source. Error.");
		return QPoint(-1, -1);
	}

	const int resultWidth = sourceWidth - targetWidth;
	const int resultHeight = sourceHeight - targetHeight;

	if (resultWidth <= 0 || resultHeight <= 0) {
		qWarning() << QString::fromUtf8("Invalid size");
		return QPoint(-1, -1);
	}

	//qDebug() << "Поиск изображения" << targetWidth << "x" << targetHeight
	//	<< "в" << sourceWidth << "x" << sourceHeight
	//	<< "порог:" << requiredSimilarity;

	// Конвертируем в float arrays
	QVector<float> source_data = ConvertGrayscaleToFloatArray(source_grayscale_img);
	QVector<float> target_data = ConvertGrayscaleToFloatArray(target_grayscale_img);

	if (source_data.isEmpty() || target_data.isEmpty())
	{
		qWarning() << QString::fromUtf8("Convert float arrays ERROR!");
		return QPoint(-1, -1);
	}

	// Буфер для результата: [found, x, y]
	int outputData[3] = { 0, -1, -1 };

	cl_int err;
	cl_mem sourceBuffer = nullptr;
	cl_mem targetBuffer = nullptr;
	cl_mem outputBuffer = nullptr;

	// Создаем буферы
	sourceBuffer = clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		source_data.size() * sizeof(float), source_data.data(), &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Create source source buffer error : ") << err;
		return QPoint(-1, -1);
	}

	targetBuffer = clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, target_data.size() * sizeof(float), target_data.data(), &err);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Create target buffer error : ") << err;
		clReleaseMemObject(sourceBuffer);
		return QPoint(-1, -1);
	}

	outputBuffer = clCreateBuffer(context_, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		3 * sizeof(int), outputData, &err);
	if (err != CL_SUCCESS) {
		qWarning() << QString::fromUtf8("Create output buffer error : ") << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		return QPoint(-1, -1);
	}

	// Устанавливаем аргументы kernel
	err = clSetKernelArg(kernel_, 0, sizeof(cl_mem), &sourceBuffer);
	err |= clSetKernelArg(kernel_, 1, sizeof(cl_mem), &targetBuffer);
	err |= clSetKernelArg(kernel_, 2, sizeof(cl_mem), &outputBuffer);
	err |= clSetKernelArg(kernel_, 3, sizeof(int), &sourceWidth);
	err |= clSetKernelArg(kernel_, 4, sizeof(int), &sourceHeight);
	err |= clSetKernelArg(kernel_, 5, sizeof(int), &targetWidth);
	err |= clSetKernelArg(kernel_, 6, sizeof(int), &targetHeight);
	float req_sim = static_cast<float>(requiredSimilarity);
	err |= clSetKernelArg(kernel_, 7, sizeof(float), &req_sim);

	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Set arguments error. kernel : ") << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		clReleaseMemObject(outputBuffer);
		return QPoint(-1, -1);
	}

	// Определяем размеры рабочих групп
	size_t maxWorkGroupSize;
	clGetDeviceInfo(device_, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr);

	size_t localWorkSize[2] = { 16, 16 };
	if (maxWorkGroupSize < 256)
	{
		localWorkSize[0] = 8;
		localWorkSize[1] = 8;
	}

	size_t globalWorkSize[2] = {
		((resultWidth + localWorkSize[0] - 1) / localWorkSize[0]) * localWorkSize[0],
		((resultHeight + localWorkSize[1] - 1) / localWorkSize[1]) * localWorkSize[1]
	};

	// Запускаем kernel
	err = clEnqueueNDRangeKernel(queue_, kernel_, 2, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Execution kernel error : d") << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		clReleaseMemObject(outputBuffer);
		return QPoint(-1, -1);
	}

	// Ждем завершения
	clFinish(queue_);

	// Читаем результат
	int finalResult[3];
	err = clEnqueueReadBuffer(queue_, outputBuffer, CL_TRUE, 0, 3 * sizeof(int), finalResult, 0, nullptr, nullptr);
	if (err != CL_SUCCESS)
	{
		qWarning() << QString::fromUtf8("Read result error : ") << err;
	}

	// Освобождаем ресурсы
	clReleaseMemObject(sourceBuffer);
	clReleaseMemObject(targetBuffer);
	clReleaseMemObject(outputBuffer);

	qDebug() << QString::fromUtf8("Detect duration : ") << timer.elapsed() << QString::fromUtf8(" ms");

	if (finalResult[0] != 0)
	{
		qDebug() << QString::fromUtf8("Found position : ") << finalResult[1] << finalResult[2] << QString::fromUtf8(", func ret ") << finalResult[0];
		return QPoint(finalResult[1], finalResult[2]);
	}
	else
	{
		qDebug() << QString::fromUtf8("Position not found");
		return QPoint(-1, -1);
	}
}

QString OpenCLImageFinder::GetDeviceInfo() const
{
	if (!device_)
	{
		return QString::fromUtf8("Device is not initialized.");
	}

	char device_name[128] = { 0 };
	clGetDeviceInfo(device_, CL_DEVICE_NAME, sizeof(device_name), device_name, nullptr);

	cl_device_type device_type;
	clGetDeviceInfo(device_, CL_DEVICE_TYPE, sizeof(device_type), &device_type, nullptr);

	QString type_str;
	if (device_type & CL_DEVICE_TYPE_GPU)
	{
		type_str = QString::fromUtf8("GPU");
	}
	else if (device_type & CL_DEVICE_TYPE_CPU)
	{
		type_str = QString::fromUtf8("CPU");
	}
	else
	{
		type_str = QString::fromUtf8("Unknown");
	}

	return QString::fromUtf8("Device: %1 (%2)").arg(device_name).arg(type_str);
}