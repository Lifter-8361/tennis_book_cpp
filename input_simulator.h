#include <QApplication>
#include <QPoint>
#include <QDebug>
#include <QThread>
#include <windows.h>

class InputSimulator : public QObject
{
    Q_OBJECT
public:
    explicit InputSimulator(QObject *parent = nullptr) : QObject(parent) {}
    
    // Перемещение мыши и клик
    bool moveAndClick(const QPoint &point, int delay_ms = 0)
    {
        MoveMouse(point);
        
        if (delay_ms)
        {
            QThread::msleep(delay_ms);
        }

        LeftClick();
        
        if (delay_ms)
        {
            QThread::msleep(delay_ms);
        }

        return true;
    }
    
    // Отправка символа и Enter
    bool sendPlusAndEnter(int delayMs = 0)
    {
        if (!sendKeyPlus())
        {
            qWarning() << "Не удалось отправить '+'";
            return false;
        }
        
        if (delayMs)
        {
            QThread::msleep(delayMs);
        }
        if (!sendEnter())
        {
            qWarning() << "Не удалось отправить Enter";
            return false;
        }
        
        QThread::msleep(delayMs);
        return true;
    }
    
    // Полная последовательность действий
    bool executeFullSequence(const QPoint &point, int delayBetweenSteps = 0)
    {
        qDebug() << "Запуск полной последовательности...";
        
        if (!moveAndClick(point, delayBetweenSteps)) {
            return false;
        }
        
        if (!sendPlusAndEnter(delayBetweenSteps)) {
            return false;
        }
        
        qDebug() << "Последовательность выполнена успешно!";
        return true;
    }

private:
    // Перемещение мыши
    void MoveMouse(const QPoint &point)
    {
        QCursor::setPos(point);
    }
    
    // Левый клик мыши
    void LeftClick()
    {
        INPUT inputs[2] = {};
        ZeroMemory(inputs, sizeof(inputs));
        
        // Нажатие левой кнопки
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        
        // Отпускание левой кнопки
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        
        UINT sent = SendInput(2, inputs, sizeof(INPUT));
        if (sent == 2)
        {
            qDebug() << "Левый клик выполнен";
        }
    }
    
    // Отправка символа
    bool sendKeyPlus()
    {
		QVector<INPUT> inputs;

		// Shift down
		INPUT shiftDown = {};
		shiftDown.type = INPUT_KEYBOARD;
		shiftDown.ki.wVk = VK_SHIFT;
		inputs.append(shiftDown);

		// Key down (клавиша =/+)
		INPUT keyDown = {};
		keyDown.type = INPUT_KEYBOARD;
		keyDown.ki.wVk = VK_OEM_PLUS;
		inputs.append(keyDown);

		// Key up
		INPUT keyUp = {};
		keyUp.type = INPUT_KEYBOARD;
		keyUp.ki.wVk = VK_OEM_PLUS;
		keyUp.ki.dwFlags = KEYEVENTF_KEYUP;
		inputs.append(keyUp);

		// Shift up
		INPUT shiftUp = {};
		shiftUp.type = INPUT_KEYBOARD;
		shiftUp.ki.wVk = VK_SHIFT;
		shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
		inputs.append(shiftUp);

		UINT sent = SendInput(inputs.size(), inputs.data(), sizeof(INPUT));
		if (sent == inputs.size()) {
			qDebug() << "Отправлен символ '+'";
			return true;
		}
    }
    
    // Отправка Enter
    bool sendEnter()
    {
        #ifdef Q_OS_WIN
        INPUT inputs[2] = {};
        ZeroMemory(inputs, sizeof(inputs));
        
        // Нажатие Enter
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_RETURN;
        
        // Отпускание Enter
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_RETURN;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        
        UINT sent = SendInput(2, inputs, sizeof(INPUT));
        if (sent == 2) {
            qDebug() << "Отправлен Enter";
            return true;
        }
        #else
        qDebug() << "Отправка Enter поддерживается только на Windows";
        #endif
        return false;
    }
};