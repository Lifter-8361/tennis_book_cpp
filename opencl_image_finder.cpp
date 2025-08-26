#include "opencl_image_finder.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>

#define CL_TARGET_OPENCL_VERSION 120

#define DEBUG_GRAYSCALE 0

OpenCLImageFinder::OpenCLImageFinder(QObject* parent)
	: QObject(parent), context(nullptr), device(nullptr),
	queue(nullptr), program(nullptr), kernel(nullptr), isInitialized(false)
{
}

OpenCLImageFinder::~OpenCLImageFinder()
{
	cleanupOpenCL();
}

void OpenCLImageFinder::cleanupOpenCL()
{
	if (kernel) clReleaseKernel(kernel);
	if (program) clReleaseProgram(program);
	if (queue) clReleaseCommandQueue(queue);
	if (context) clReleaseContext(context);

	kernel = nullptr;
	program = nullptr;
	queue = nullptr;
	context = nullptr;
	isInitialized = false;
}

bool OpenCLImageFinder::initializeOpenCL()
{
	if (isInitialized) return true;

	cl_int err;
	cl_platform_id platform;

	err = clGetPlatformIDs(1, &platform, nullptr);
	if (err != CL_SUCCESS) {
		qWarning() << "Ошибка получения платформы OpenCL:" << err;
		return false;
	}

	cl_device_type deviceType = CL_DEVICE_TYPE_GPU;
	err = clGetDeviceIDs(platform, deviceType, 1, &device, nullptr);
	if (err != CL_SUCCESS) {
		qWarning() << "GPU не найден, пробуем CPU...";
		deviceType = CL_DEVICE_TYPE_CPU;
		err = clGetDeviceIDs(platform, deviceType, 1, &device, nullptr);
		if (err != CL_SUCCESS) {
			qWarning() << "Ошибка получения устройства OpenCL:" << err;
			return false;
		}
	}

	context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Ошибка создания контекста:" << err;
		return false;
	}

	queue = clCreateCommandQueue(context, device, 0, &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Ошибка создания очереди команд:" << err;
		cleanupOpenCL();
		return false;
	}
	printDeviceInfo();

	if (!compileKernel()) {
		cleanupOpenCL();
		return false;
	}

	printDeviceInfo();
	isInitialized = true;
	return true;
}

bool OpenCLImageFinder::compileKernel()
{
	const char* kernelSource = R"(
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
	program = clCreateProgramWithSource(context, 1, &kernelSource, nullptr, &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Create program error : " << err;
		return false;
	}

	err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
	if (err != CL_SUCCESS) {
		qWarning() << "Compile program error. kernel : " << err;

		size_t logSize;
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
		QVector<char> log(logSize);
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
		qWarning() << "Compilation log : " << log.data();

		return false;
	}

	kernel = clCreateKernel(program, "findFirstMatchMinimal", &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Creation error. kernel : " << err;
		return false;
	}

	return true;
}

void OpenCLImageFinder::printDeviceInfo() const
{
	if (!device) return;

	char deviceName[128] = { 0 };
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);

	cl_ulong globalMemSize;
	clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(globalMemSize), &globalMemSize, nullptr);

	size_t maxWorkGroupSize;
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr);

	qDebug() << "OpenCL Device : " << deviceName;
	qDebug() << "Global Memory : " << globalMemSize / (1024 * 1024) << " MB";
	qDebug() << "Max Work Group Size : " << maxWorkGroupSize;
}

QImage OpenCLImageFinder::convertToGrayscale(const QImage& image)
{
	if (image.isNull()) return QImage();
	if (image.format() == QImage::Format_Grayscale8) {
		return image;
	}
	return image.convertToFormat(QImage::Format_Grayscale8);
}

QVector<float> OpenCLImageFinder::convertGrayscaleToFloatArray(const QImage& grayscaleImage)
{
	if (grayscaleImage.isNull() || grayscaleImage.format() != QImage::Format_Grayscale8) {
		return QVector<float>();
	}

	const int width = grayscaleImage.width();
	const int height = grayscaleImage.height();
	QVector<float> array(width * height);

	for (int y = 0; y < height; y++) {
		const uchar* scanLine = grayscaleImage.constScanLine(y);
		for (int x = 0; x < width; x++) {
			float float_v = static_cast<float>(scanLine[x]);
			array[y * width + x] = static_cast<float>(scanLine[x]) / 255.0f;
		}
	}

	return array;
}

#if DEBUG_GRAYSCALE
	#include <QLabel>
#endif

QPoint OpenCLImageFinder::findFirstMatchMinimal(const QImage& source, const QImage& target,
	double requiredSimilarity)
{
	if (!isInitialized && !initializeOpenCL())
	{
		qWarning() << "OpenCL is not initialized.";
		return QPoint(-1, -1);
	}

	if (source.isNull() || target.isNull()) {
		qWarning() << "images not loaded.";
		return QPoint(-1, -1);
	}

	QElapsedTimer timer;
	timer.start();

	// Конвертируем в grayscale
	QImage sourceGray = convertToGrayscale(source);
	QImage targetGray = convertToGrayscale(target);

	if (sourceGray.isNull() || targetGray.isNull()) {
		qWarning() << "Convert to grayscale error!";
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

	display_image("sourceGray", sourceGray);
	display_image("targetGray", targetGray);
	display_image("source", source);
	display_image("target", target);
#endif

	const int sourceWidth = sourceGray.width();
	const int sourceHeight = sourceGray.height();
	const int targetWidth = targetGray.width();
	const int targetHeight = targetGray.height();

	if (targetWidth > sourceWidth || targetHeight > sourceHeight) {
		qWarning() << "Target image greater than source. Error.";
		return QPoint(-1, -1);
	}

	const int resultWidth = sourceWidth - targetWidth;
	const int resultHeight = sourceHeight - targetHeight;

	if (resultWidth <= 0 || resultHeight <= 0) {
		qWarning() << "Invalid size";
		return QPoint(-1, -1);
	}

	//qDebug() << "Поиск изображения" << targetWidth << "x" << targetHeight
	//	<< "в" << sourceWidth << "x" << sourceHeight
	//	<< "порог:" << requiredSimilarity;

	// Конвертируем в float arrays
	QVector<float> sourceData = convertGrayscaleToFloatArray(sourceGray);
	QVector<float> targetData = convertGrayscaleToFloatArray(targetGray);

	if (sourceData.isEmpty() || targetData.isEmpty()) {
		qWarning() << "Convert float arrays ERROR!";
		return QPoint(-1, -1);
	}

	// Буфер для результата: [found, x, y]
	int outputData[3] = { 0, -1, -1 };

	cl_int err;
	cl_mem sourceBuffer = nullptr;
	cl_mem targetBuffer = nullptr;
	cl_mem outputBuffer = nullptr;

	// Создаем буферы
	sourceBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sourceData.size() * sizeof(float), sourceData.data(), &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Create source source buffer error : " << err;
		return QPoint(-1, -1);
	}

	targetBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		targetData.size() * sizeof(float), targetData.data(), &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Create target buffer error : " << err;
		clReleaseMemObject(sourceBuffer);
		return QPoint(-1, -1);
	}

	outputBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		3 * sizeof(int), outputData, &err);
	if (err != CL_SUCCESS) {
		qWarning() << "Create output buffer error : " << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		return QPoint(-1, -1);
	}

	// Устанавливаем аргументы kernel
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &sourceBuffer);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &targetBuffer);
	err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &outputBuffer);
	err |= clSetKernelArg(kernel, 3, sizeof(int), &sourceWidth);
	err |= clSetKernelArg(kernel, 4, sizeof(int), &sourceHeight);
	err |= clSetKernelArg(kernel, 5, sizeof(int), &targetWidth);
	err |= clSetKernelArg(kernel, 6, sizeof(int), &targetHeight);
	float req_sim = static_cast<float>(requiredSimilarity);
	err |= clSetKernelArg(kernel, 7, sizeof(float), &req_sim);

	if (err != CL_SUCCESS) {
		qWarning() << "Set arguments error. kernel : " << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		clReleaseMemObject(outputBuffer);
		return QPoint(-1, -1);
	}

	// Определяем размеры рабочих групп
	size_t maxWorkGroupSize;
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr);

	size_t localWorkSize[2] = { 16, 16 };
	if (maxWorkGroupSize < 256) {
		localWorkSize[0] = 8;
		localWorkSize[1] = 8;
	}

	size_t globalWorkSize[2] = {
		((resultWidth + localWorkSize[0] - 1) / localWorkSize[0]) * localWorkSize[0],
		((resultHeight + localWorkSize[1] - 1) / localWorkSize[1]) * localWorkSize[1]
	};

	// Запускаем kernel
	err = clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) {
		qWarning() << "Execution kernel error : d" << err;
		clReleaseMemObject(sourceBuffer);
		clReleaseMemObject(targetBuffer);
		clReleaseMemObject(outputBuffer);
		return QPoint(-1, -1);
	}

	// Ждем завершения
	clFinish(queue);

	// Читаем результат
	int finalResult[3];
	err = clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0, 3 * sizeof(int), finalResult, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) {
		qWarning() << "Read result error : " << err;
	}

	// Освобождаем ресурсы
	clReleaseMemObject(sourceBuffer);
	clReleaseMemObject(targetBuffer);
	clReleaseMemObject(outputBuffer);

	qDebug() << "Detect duration : " << timer.elapsed() << " ms";

	if (finalResult[0] != 0) {
		qDebug() << "Found position : " << finalResult[1] << finalResult[2] << ", func ret " << finalResult[0];
		return QPoint(finalResult[1], finalResult[2]);
	}
	else {
		qDebug() << "Position not found";
		return QPoint(-1, -1);
	}
}

QString OpenCLImageFinder::getDeviceInfo() const
{
	if (!device) return "Device is not initialized.";

	char deviceName[128] = { 0 };
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);

	cl_device_type deviceType;
	clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(deviceType), &deviceType, nullptr);

	QString typeStr;
	if (deviceType & CL_DEVICE_TYPE_GPU) typeStr = "GPU";
	else if (deviceType & CL_DEVICE_TYPE_CPU) typeStr = "CPU";
	else typeStr = "Unknown";

	return QString("Device: %1 (%2)").arg(deviceName).arg(typeStr);
}